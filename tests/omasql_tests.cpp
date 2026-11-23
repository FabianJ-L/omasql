#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QtTest>

#include "backend.h"
#include "database.h"
#include "filepicker.h"
#include "highlighter.h"
#include "sqltext.h"
#include "theme.h"
#include "translator.h"

class FakePicker : public FilePicker {
    Q_OBJECT
public:
    QString next;
    void openDatabase(const QString &) override { emit picked(next); }
    void saveFile(const QString &, const QString &, const QString &, const QString &) override { emit picked(next); }
};

class OmasqlTests : public QObject {
    Q_OBJECT

private slots:
    void splitsStatements()
    {
        const auto parts = SqlText::split("select 1; select ';' -- ;\n; select \"a;b\"");
        QCOMPARE(parts.size(), 3);
        QCOMPARE(parts[0].text, QString("select 1"));
        QCOMPARE(parts[1].text, QString("select ';' -- ;"));
        QCOMPARE(parts[2].text, QString("select \"a;b\""));
    }

    void keepsTriggerBodiesTogether()
    {
        const auto parts = SqlText::split("CREATE TRIGGER t AFTER INSERT ON a BEGIN UPDATE b SET x = 1; DELETE FROM c; END; SELECT 1;");
        QCOMPARE(parts.size(), 2);
        QVERIFY(parts[0].text.endsWith("END"));
    }

    void findsStatementUnderCursor()
    {
        const QString sql = "select 1;\nselect 2;\n\n";
        QCOMPARE(SqlText::statementAt(sql, 3).text, QString("select 1"));
        QCOMPARE(SqlText::statementAt(sql, 9).text, QString("select 1"));  // right after the semicolon
        QCOMPARE(SqlText::statementAt(sql, 12).text, QString("select 2"));
        QCOMPARE(SqlText::statementAt(sql, sql.size()).text, QString("select 2")); // trailing blank lines
    }

    void knowsLiterals()
    {
        const QString sql = "select 'abc', x -- note\nfrom t";
        QVERIFY(SqlText::insideLiteral(sql, 9));
        QVERIFY(!SqlText::insideLiteral(sql, 14));
        QVERIFY(SqlText::insideLiteral(sql, 20));
        QVERIFY(!SqlText::insideLiteral(sql, sql.size()));
    }

    void readsCompletionContext()
    {
        int start = 0;
        const QString sql = "select o.na";
        QCOMPARE(SqlText::wordBefore(sql, sql.size(), &start), QString("na"));
        QCOMPARE(SqlText::qualifierBefore(sql, start), QString("o"));
        QCOMPARE(SqlText::previousKeyword("select * from a, ", 17), QString("FROM"));
        QCOMPARE(SqlText::previousKeyword("select * from a ", 16), QString());

        const QStringList tables{"orders", "Customers"};
        const auto aliases = SqlText::aliases("select * from orders o join customers AS c on c.id = o.cid", tables);
        QCOMPARE(aliases.value("o"), QString("orders"));
        QCOMPARE(aliases.value("c"), QString("Customers"));
        QCOMPARE(aliases.value("orders"), QString("orders"));
    }

    void findsTablesAfterColumnLists()
    {
        const QStringList tables{"orders", "customers"};
        QCOMPARE(SqlText::referencedTables("select id, total from orders", tables), QStringList{"orders"});
        QCOMPARE(SqlText::referencedTables("select a, b from orders o join customers c on c.id = o.customer", tables),
                 QStringList({"orders", "customers"}));
        QCOMPARE(SqlText::aliases("select x, y from orders o", tables).value("o"), QString("orders"));
    }

    void spotsDestructiveStatements()
    {
        QVERIFY(SqlText::isDestructive("drop table x"));
        QVERIFY(SqlText::isDestructive("select 1; delete from x"));
        QVERIFY(SqlText::isDestructive("UPDATE x SET a = 1"));
        QVERIFY(!SqlText::isDestructive("UPDATE x SET a = 1 WHERE id = 2"));
        QVERIFY(!SqlText::isDestructive("select 'drop table x'"));
        QVERIFY(!SqlText::isDestructive("-- drop table x\nselect 1"));
    }

    void quotesIdentifiers()
    {
        QCOMPARE(SqlText::identifier("name"), QString("name"));
        QCOMPARE(SqlText::identifier("order"), QString("\"order\""));
        QCOMPARE(SqlText::identifier("first name"), QString("\"first name\""));
    }

    void executesQueries()
    {
        QTemporaryDir dir;
        Database db;
        QString error;
        QVERIFY2(db.open(dir.filePath("test.db"), &error), qPrintable(error));
        auto r = db.execute("create table t (id integer primary key, name text, score real, data blob);"
                            "insert into t (name, score, data) values ('a', 1.5, x'00ff'), (null, 2, null);"
                            "select * from t order by id", 100);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.statements, 3);
        QCOMPARE(r.columns, QStringList({"id", "name", "score", "data"}));
        QCOMPARE(r.types.value(0), QString("INTEGER"));
        QCOMPARE(r.rows.size(), 2);
        QCOMPARE(r.rows[0][1].toString(), QString("a"));
        QVERIFY(r.rows[1][1].isNull());
        QCOMPARE(r.rows[0][3].toByteArray(), QByteArray::fromHex("00ff"));

        r = db.execute("select * from t", 1);
        QVERIFY(r.truncated);
        QCOMPARE(r.rows.size(), 1);

        r = db.execute("update t set score = 0", 10);
        QCOMPARE(r.changes, 2);
        QVERIFY(!r.hasRows);

        r = db.execute("select 1;\nselect nope from t", 10);
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains("nope"));
        QCOMPARE(r.errorOffset, 17); // points at "nope"

        const auto schema = db.schema();
        QCOMPARE(schema.size(), 1);
        QCOMPARE(schema[0].rows, 2);
        QVERIFY(schema[0].columns[0].primaryKey);
    }

    void rejectsNonDatabases()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("notes.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("this is not a database, just some text that is long enough to have a header");
        file.close();
        Database db;
        QString error;
        QVERIFY(!db.open(file.fileName(), &error));
        QVERIFY(!error.isEmpty());
    }

    void sortsAndFormatsResults()
    {
        ResultModel model;
        model.setResult({"v"}, {""}, {{QVariant::fromValue<qint64>(10)}, {QVariant()}, {QString("b")},
                                      {2.5}, {QString("{\"a\":1}")}});
        model.sortBy(0);
        QCOMPARE(model.cellText(0, 0), QString());
        QCOMPARE(model.cellText(1, 0), QString("2.5"));
        QCOMPARE(model.cellText(2, 0), QString("10"));
        model.sortBy(0);
        QCOMPARE(model.cellText(4, 0), QString());
        bool json = false;
        QVERIFY(ResultModel::fullText(QString("{\"a\":1}"), &json).contains("\n"));
        QVERIFY(json);
        QCOMPARE(ResultModel::displayText(QVariant()), QString("NULL"));
    }

    void completesFromTheSchema()
    {
        QTemporaryDir dir;
        FakePicker picker;
        Backend backend(&picker, dir.filePath("state"));
        QVERIFY(backend.openDatabase(dir.filePath("shop.db")));
        QSignalSpy finished(&backend, &Backend::queryFinished);
        backend.run("create table orders (id integer primary key, total real, customer_id integer);"
                    "create table customers (id integer primary key, name text)");
        QVERIFY(finished.wait());

        auto labels = [&](const QString &text, bool forced = false) {
            QStringList out;
            for (const auto &item : backend.completions(text, text.size(), forced).value("items").toList())
                out << item.toMap().value("label").toString();
            return out;
        };
        QCOMPARE(labels("select * from ord").value(0), QString("orders"));
        auto at = [&](const QString &text, int position) {
            QStringList out;
            for (const auto &item : backend.completions(text, position, false).value("items").toList())
                out << item.toMap().value("label").toString();
            return out;
        };
        QCOMPARE(at("select o. from orders o", 9), QStringList({"id", "total", "customer_id"}));
        QCOMPARE(labels("select orders."), QStringList({"id", "total", "customer_id"}));
        QCOMPARE(at("select c.na from orders o join customers c", 11), QStringList({"name"}));
        QVERIFY(labels("select na from customers").isEmpty()); // cursor after "customers"
        const QString columns = "select tot from orders";
        QStringList found;
        for (const auto &item : backend.completions(columns, 10, false).value("items").toList())
            found << item.toMap().value("label").toString();
        QCOMPARE(found.value(0), QString("total"));
        QCOMPARE(labels("SEL").value(0), QString("SELECT"));
        QVERIFY(labels("select 'ord").isEmpty());
    }

    void remembersHistoryAndDrafts()
    {
        QTemporaryDir dir;
        FakePicker picker;
        {
            Backend backend(&picker, dir.filePath("state"));
            picker.next = dir.filePath("a.db");
            backend.chooseDatabase();
            QVERIFY(backend.isOpen());
            QSignalSpy finished(&backend, &Backend::queryFinished);
            backend.run("select 42");
            QVERIFY(finished.wait());
            QCOMPARE(backend.results()->cellText(0, 0), QString("42"));
            backend.saveDraft("select 42;");
        }
        Backend again(&picker, dir.filePath("state"));
        QCOMPARE(again.history().value(0).toMap().value("sql").toString(), QString("select 42"));
        QCOMPARE(again.recent().value(0).toMap().value("name").toString(), QString("a.db"));
        QVERIFY(again.openDatabase(dir.filePath("a.db")));
        QCOMPARE(again.draft(), QString("select 42;"));
    }

    void cancelsLongQueries()
    {
        QTemporaryDir dir;
        FakePicker picker;
        Backend backend(&picker, dir.filePath("state"));
        QVERIFY(backend.openDatabase(":memory:"));
        QSignalSpy finished(&backend, &Backend::queryFinished);
        backend.run("with recursive c(x) as (select 1 union all select x + 1 from c) select count(*) from c");
        QTest::qWait(100);
        QVERIFY(backend.running());
        backend.cancel();
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().first().toBool(), false);
        QCOMPARE(backend.statusKind(), QString("warning"));
    }

    void exportsCsv()
    {
        QTemporaryDir dir;
        FakePicker picker;
        Backend backend(&picker, dir.filePath("state"));
        QVERIFY(backend.openDatabase(":memory:"));
        QSignalSpy finished(&backend, &Backend::queryFinished);
        backend.run("select 1 as a, 'x,y' as b, null as c");
        QVERIFY(finished.wait());
        picker.next = dir.filePath("out.csv");
        backend.exportCsv();
        QFile file(picker.next);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("a,b,c\r\n1,\"x,y\",\r\n"));
    }

    void filtersAndFormatsResults()
    {
        ResultModel model;
        model.setResult({"id", "name", "data"}, {"INTEGER", "TEXT", "BLOB"},
                        {{QVariant::fromValue<qint64>(1), QString("Ada Lovelace"), QByteArray::fromHex("00ff")},
                         {QVariant::fromValue<qint64>(2), QString("it's, \"quoted\""), QVariant()},
                         {QVariant::fromValue<qint64>(3), QString("Alan Turing"), QVariant()}});
        model.setFilter("ada");
        QCOMPARE(model.rowTotal(), 1);
        QCOMPARE(model.totalRows(), 3);
        model.setFilter("a  TURING"); // every word, any column, any case
        QCOMPARE(model.cellText(0, 0), QString("3"));
        model.setFilter("null");
        QCOMPARE(model.rowTotal(), 2);
        model.sortBy(0);
        model.sortBy(0); // descending, and still filtered
        QCOMPARE(model.cellText(0, 0), QString("3"));
        model.setFilter("");
        QCOMPARE(model.rowTotal(), 3);
        QCOMPARE(model.rangeText(0, 0, 1, 1), QString("3\tAlan Turing\n2\tit's, \"quoted\""));

        model.sortBy(0); // back to ascending
        QCOMPARE(model.toSqlInsert("people").section('\n', 0, 1),
                 QString("INSERT INTO \"people\" (\"id\", \"name\", \"data\") VALUES (1, 'Ada Lovelace', X'00ff');\n"
                         "INSERT INTO \"people\" (\"id\", \"name\", \"data\") VALUES (2, 'it''s, \"quoted\"', NULL);"));
        const auto json = QJsonDocument::fromJson(model.toJson().toUtf8()).array();
        QCOMPARE(json.size(), 3);
        QCOMPARE(json[0].toObject().value("id").toInteger(), 1);
        QVERIFY(json[1].toObject().value("data").isNull());
        QCOMPARE(model.toMarkdown().section('\n', 0, 2),
                 QString("| id | name | data |\n| --- | --- | --- |\n| 1 | Ada Lovelace | 00ff |"));
        QCOMPARE(ResultModel::sqlLiteral(2.0), QString("2.0"));
    }

    void followsForeignKeysAndExplains()
    {
        QTemporaryDir dir;
        Backend backend(nullptr, dir.filePath("state"));
        QVERIFY(backend.openDatabase(":memory:"));
        QSignalSpy finished(&backend, &Backend::queryFinished);
        backend.run("create table customers (id integer primary key, name text);"
                    "create table orders (id integer primary key, customer integer references customers, total real);"
                    "insert into customers values (7, 'Grace');"
                    "insert into orders values (1, 7, 9.5)");
        QVERIFY(finished.wait());

        backend.preview("orders");
        QVERIFY(finished.wait());
        QCOMPARE(backend.resultTable(), QString("orders"));
        QCOMPARE(backend.referenceOf("customer"), QString("customers"));
        backend.followReference(0, 1);
        QVERIFY(finished.wait());
        QCOMPARE(backend.resultTable(), QString("customers"));
        QCOMPARE(backend.results()->cellText(0, 1), QString("Grace"));

        // A SELECT over one table counts as that table; a computed column doesn't.
        backend.run("select id, customer from orders");
        QVERIFY(finished.wait());
        QCOMPARE(backend.resultTable(), QString("orders"));
        backend.run("select count(*) from orders");
        QVERIFY(finished.wait());
        QCOMPARE(backend.resultTable(), QString());

        backend.explain("select * from orders where customer = 7; delete from orders");
        QVERIFY(finished.wait());
        QCOMPARE(backend.results()->columnTotal(), 1);
        QVERIFY(backend.results()->cellText(0, 0).contains("orders"));
        backend.run("select count(*) from orders");
        QVERIFY(finished.wait());
        QCOMPARE(backend.results()->cellText(0, 0), QString("1")); // the DELETE never ran
    }

    void undoesAnEditInOneStep()
    {
        QTextDocument document;
        document.setPlainText("selct name from user");
        document.clearUndoRedoStacks();

        // A completion: the typed word out, the chosen one in.
        SqlHighlighter::replaceAsOneStep(&document, 0, 5, "SELECT");
        QCOMPARE(document.toPlainText(), QString("SELECT name from user"));
        // A comment toggle over the whole line.
        SqlHighlighter::replaceAsOneStep(&document, 0, 21, "-- SELECT name from user");
        QCOMPARE(document.toPlainText(), QString("-- SELECT name from user"));

        document.undo();
        QCOMPARE(document.toPlainText(), QString("SELECT name from user"));
        document.undo();
        QCOMPARE(document.toPlainText(), QString("selct name from user"));
        QVERIFY(!document.isUndoAvailable());
        // One Ctrl+Z too many is not the end: redo brings it all back.
        document.redo();
        document.redo();
        QCOMPARE(document.toPlainText(), QString("-- SELECT name from user"));

        // Out-of-range positions are clamped, not a crash.
        SqlHighlighter::replaceAsOneStep(&document, 50, 99, ";");
        QCOMPARE(document.toPlainText(), QString("-- SELECT name from user;"));
    }

    void movesStateFromTheOldName()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("lode");
        write(dir.path() + "/lode/recent.json", "[\"/tmp/a.db\"]");
        qputenv("XDG_STATE_HOME", dir.path().toUtf8());
        {
            Backend backend(nullptr);
            QCOMPARE(backend.recent().size(), 1);
        }
        qunsetenv("XDG_STATE_HOME");
        QVERIFY(QFileInfo::exists(dir.path() + "/omasql/recent.json"));
        QVERIFY(!QFileInfo::exists(dir.path() + "/lode"));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
        QVERIFY(theme.colors().contains("red"));
    }

    void themeReadsPalette()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\nbackground = \"#010203\"\ncolor1 = \"#ff0000\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
        QCOMPARE(theme.colors().value("background").toString(), QString("#010203"));
        QCOMPARE(theme.colors().value("red").toString(), QString("#ff0000"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");
        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "de\n");
        write(dir.path() + "/en.json", R"({"%n rows": {"one": "%n row", "other": "%n rows"}})");
        write(dir.path() + "/de.json", R"({"Run": "Ausführen", "%n rows": {"one": "%n Zeile", "other": "%n Zeilen"}})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "Run"), QString("Ausführen"));
        QCOMPARE(translator.translate("Main", "%n rows", nullptr, 1), QString("%n Zeile"));
        QCOMPARE(translator.translate("Main", "%n rows", nullptr, 3), QString("%n Zeilen"));
    }

    void shipsCompleteCatalogs()
    {
        // Every catalog compiled in parses, so a typo can't silently drop a language.
        for (const QString &path : QDir(QFINDTESTDATA("../src/i18n")).entryList({"*.json"})) {
            QFile file(QFINDTESTDATA("../src/i18n/" + path));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QJsonParseError error;
            QJsonDocument::fromJson(file.readAll(), &error);
            QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(path + ": " + error.errorString()));
        }
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(OmasqlTests)
#include "omasql_tests.moc"
