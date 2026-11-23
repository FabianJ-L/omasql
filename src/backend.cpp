#include "backend.h"

#include "filepicker.h"
#include "sqltext.h"

#include <QClipboard>
#include <algorithm>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

namespace {

const int HistoryLimit = 200;
const int RecentLimit = 12;

QString defaultStateDirectory() {
    QString base = qEnvironmentVariable("XDG_STATE_HOME");
    if (base.isEmpty())
        base = QDir::homePath() + "/.local/state";
    // The app was called Lode, then SQLite Lens; its history, recent files,
    // and drafts move over the first time it starts under the new name.
    const QString directory = base + "/omasql";
    for (const char *previous : {"/sqlite-lens", "/lode"})
        if (!QFileInfo::exists(directory) && QFileInfo(base + previous).isDir())
            QDir().rename(base + previous, directory);
    return directory;
}

QString duration(qint64 ms) {
    if (ms < 1000)
        return Backend::tr("%1 ms").arg(ms);
    return Backend::tr("%1 s").arg(QLocale().toString(ms / 1000.0, 'f', ms < 10000 ? 2 : 1));
}

} // namespace

Backend::Backend(FilePicker *picker, const QString &stateDirectory, QObject *parent)
    : QObject(parent), m_picker(picker),
      m_stateDirectory(stateDirectory.isEmpty() ? defaultStateDirectory() : stateDirectory) {
    connect(&m_watcher, &QFutureWatcher<QueryResult>::finished, this, &Backend::finished);

    if (m_picker) {
        connect(m_picker, &FilePicker::picked, this, [this](const QString &path) {
            const Pending pending = std::exchange(m_pending, Pending::None);
            if (pending == Pending::Open) {
                openDatabase(path);
            } else if (pending == Pending::Export) {
                QString error;
                if (writeFile(path, m_results.format(m_exportFormat, m_resultTable).toUtf8(), &error))
                    setNotice(tr("Saved %1").arg(QFileInfo(path).fileName()));
                else
                    setNotice(tr("Couldn't save %1: %2").arg(QFileInfo(path).fileName(), error));
            }
        });
        connect(m_picker, &FilePicker::cancelled, this, [this] { m_pending = Pending::None; });
        connect(m_picker, &FilePicker::failed, this, [this](const QString &message) {
            m_pending = Pending::None;
            setNotice(message);
        });
    }

    QFile recent(statePath("recent.json"));
    if (recent.open(QIODevice::ReadOnly))
        for (const auto &value : QJsonDocument::fromJson(recent.readAll()).array())
            m_recent << value.toString();
    QFile history(statePath("history.json"));
    if (history.open(QIODevice::ReadOnly))
        m_history = QJsonDocument::fromJson(history.readAll()).array().toVariantList();
}

Backend::~Backend() {
    cancel();
    m_watcher.waitForFinished();
}

QString Backend::databaseName() const {
    if (!isOpen())
        return {};
    if (m_db.path() == ":memory:")
        return tr("Scratch database");
    return QFileInfo(m_db.path()).fileName();
}

QVariantList Backend::tables() const {
    QVariantList out;
    for (const auto &table : m_schema) {
        QVariantList columns;
        for (const auto &c : table.columns)
            columns << QVariantMap{{"name", c.name}, {"type", c.type}, {"primaryKey", c.primaryKey},
                                   {"notNull", c.notNull}, {"defaultValue", c.defaultValue},
                                   {"references", c.references}};
        out << QVariantMap{{"name", table.name}, {"kind", table.kind}, {"rows", table.rows},
                           {"columns", columns}, {"indexes", table.indexes}, {"sql", table.sql}};
    }
    return out;
}

QStringList Backend::tableNames() const {
    QStringList names;
    for (const auto &table : m_schema)
        names << table.name;
    return names;
}

QVariantList Backend::recent() const {
    QVariantList out;
    for (const auto &path : m_recent) {
        const QFileInfo info(path);
        out << QVariantMap{{"path", path}, {"name", info.fileName()},
                           {"folder", QDir::toNativeSeparators(info.absolutePath()).replace(QDir::homePath(), "~")},
                           {"exists", info.exists()}};
    }
    return out;
}

bool Backend::openDatabase(const QString &rawPath) {
    QString path = rawPath.trimmed();
    if (path.startsWith("file://"))
        path = QUrl(path).toLocalFile();
    if (path.startsWith("~/"))
        path = QDir::homePath() + path.mid(1);
    if (path != ":memory:" && !path.startsWith("file:"))
        path = QFileInfo(path).absoluteFilePath();

    cancel();
    m_watcher.waitForFinished();

    QString error;
    if (!m_db.open(path, &error)) {
        setNotice(tr("Couldn't open %1: %2").arg(QFileInfo(path).fileName(), error));
        return false;
    }
    if (path != ":memory:")
        rememberDatabase(path);
    m_results.clear();
    m_hasResults = false;
    m_lastSql.clear();
    m_lastError.clear();
    m_resultTable.clear();
    setStatus({}, {});
    clearNotice();
    refreshSchema();
    emit databaseChanged();
    return true;
}

void Backend::chooseDatabase() {
    if (!m_picker)
        return;
    m_pending = Pending::Open;
    m_picker->openDatabase(tr("Open SQLite database"));
}

void Backend::openScratch() { openDatabase(":memory:"); }

void Backend::closeDatabase() {
    cancel();
    m_watcher.waitForFinished();
    m_db.close();
    m_schema.clear();
    m_results.clear();
    m_hasResults = false;
    m_lastSql.clear();
    m_lastError.clear();
    m_resultTable.clear();
    setStatus({}, {});
    emit schemaChanged();
    emit databaseChanged();
}

void Backend::forgetRecent(const QString &path) {
    if (m_recent.removeAll(path) == 0)
        return;
    writeFile(statePath("recent.json"), QJsonDocument(QJsonArray::fromStringList(m_recent)).toJson());
    emit recentChanged();
}

void Backend::refreshSchema() {
    if (running())
        return;
    m_schema = m_db.schema();
    emit schemaChanged();
}

void Backend::run(const QString &sql) { start(sql, true); }

void Backend::preview(const QString &table) {
    start("SELECT * FROM " + SqlText::identifier(table), false, table);
}

// SQLite explains one statement at a time, and anything after the first
// would run for real, so only the first one goes.
void Backend::explain(const QString &sql) {
    for (const auto &segment : SqlText::split(sql)) {
        if (segment.blank)
            continue;
        if (segment.text.startsWith("EXPLAIN", Qt::CaseInsensitive))
            start(segment.text, false);
        else
            start("EXPLAIN QUERY PLAN " + segment.text, false);
        return;
    }
}

const TableInfo *Backend::findTable(const QString &name) const {
    for (const auto &t : m_schema)
        if (t.name.compare(name, Qt::CaseInsensitive) == 0)
            return &t;
    return nullptr;
}

QString Backend::referenceOf(const QString &column) const {
    if (const TableInfo *table = findTable(m_resultTable))
        for (const auto &c : table->columns)
            if (c.name == column)
                return c.references;
    return {};
}

void Backend::followReference(int row, int column) {
    const QString reference = referenceOf(m_results.columnName(column));
    const QVariant value = m_results.value(row, column);
    if (reference.isEmpty() || value.isNull())
        return;
    const int dot = reference.indexOf('.');
    const QString table = dot < 0 ? reference : reference.left(dot);
    QString target = dot < 0 ? QString() : reference.mid(dot + 1);
    // A foreign key without a column points at the other table's primary key.
    if (target.isEmpty()) {
        target = "rowid";
        if (const TableInfo *info = findTable(table))
            for (const auto &c : info->columns)
                if (c.primaryKey) {
                    target = c.name;
                    break;
                }
    }
    start("SELECT * FROM " + SqlText::identifier(table) + " WHERE " + SqlText::identifier(target)
              + " = " + ResultModel::sqlLiteral(value), false, table);
}

void Backend::start(const QString &sql, bool rememberIt, const QString &table) {
    if (!isOpen() || running() || sql.trimmed().isEmpty())
        return;
    if (rememberIt)
        remember(sql.trimmed());
    m_runningTable = table;
    m_lastSql = sql.trimmed();
    setStatus(tr("Running…"), "running");
    const Database *db = &m_db;
    m_watcher.setFuture(QtConcurrent::run([db, sql] { return db->execute(sql, MaxRows); }));
    emit runningChanged();
}

void Backend::cancel() {
    if (running())
        m_db.interrupt();
}

void Backend::waitForIdle() { m_watcher.waitForFinished(); }

void Backend::finished() {
    const QueryResult r = m_watcher.result();
    emit runningChanged();

    if (r.hasRows) {
        QStringList columns = r.columns, types = r.types;
        QList<QVariantList> rows = r.rows;
        // A query plan reads as the tree the sqlite3 shell draws, not as
        // rows of ids pointing at their parents.
        if (columns == QStringList{"id", "parent", "notused", "detail"}) {
            QHash<qint64, int> depth;
            QList<QVariantList> plan;
            for (const auto &step : rows) {
                const int d = depth.value(step.value(1).toLongLong(), -1) + 1;
                depth.insert(step.value(0).toLongLong(), d);
                plan << QVariantList{QString(d * 3, ' ') + QStringLiteral("└─ ") + step.value(3).toString()};
            }
            columns = {tr("Query plan")};
            types = {QString()};
            rows = plan;
        }
        m_results.setResult(columns, types, rows);
        m_hasResults = true;
        m_resultTable = m_runningTable;
        // A plain SELECT over one table counts as that table's rows when
        // every column it returns belongs to the table.
        if (m_resultTable.isEmpty()) {
            const auto segments = SqlText::split(m_lastSql);
            for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
                if (it->blank)
                    continue;
                const QStringList tables = SqlText::referencedTables(it->text, tableNames());
                if (it->text.startsWith("SELECT", Qt::CaseInsensitive) && tables.size() == 1)
                    if (const TableInfo *table = findTable(tables.first())) {
                        QSet<QString> names;
                        for (const auto &c : table->columns)
                            names.insert(c.name);
                        if (std::all_of(columns.begin(), columns.end(), [&](const QString &c) { return names.contains(c); }))
                            m_resultTable = table->name;
                    }
                break;
            }
        }
    } else if (r.ok) {
        m_results.clear();
        m_hasResults = false;
        m_resultTable.clear();
    }

    QString text, kind = "ok";
    m_lastError = r.ok || r.interrupted ? QString() : r.error;
    if (!r.ok) {
        text = r.interrupted ? tr("Cancelled") : r.error;
        kind = r.interrupted ? "warning" : "error";
    } else if (r.hasRows) {
        text = tr("%n rows", "", int(r.rows.size()));
        if (r.truncated)
            text += " · " + tr("only the first %1 are shown").arg(QLocale().toString(MaxRows));
    } else if (r.changes > 0) {
        text = tr("%n rows changed", "", int(r.changes));
    } else {
        text = tr("Done");
    }
    setStatus(text + " · " + duration(r.elapsedMs), kind);

    // Any statement may have changed tables or row counts.
    refreshSchema();
    emit queryFinished(r.ok, r.errorOffset);
}

QVariantMap Backend::completions(const QString &text, int position, bool forced) const {
    position = qBound(0, position, int(text.size()));
    int start = position;
    const QString prefix = SqlText::wordBefore(text, position, &start);
    QVariantMap out{{"start", start}, {"items", QVariantList()}};
    if (!isOpen() && !forced && prefix.isEmpty())
        return out;
    if (SqlText::insideLiteral(text, position))
        return out;

    const QString qualifier = SqlText::qualifierBefore(text, start);
    if (prefix.isEmpty() && qualifier.isEmpty() && !forced)
        return out;

    struct Item {
        QString label, insert, kind, detail;
        int score, order;
    };
    QList<Item> items;
    auto add = [&](const QString &label, const QString &insert, const QString &kind, const QString &detail, int order) {
        int score = 0;
        if (!prefix.isEmpty()) {
            if (label.startsWith(prefix, Qt::CaseInsensitive))
                score = 0;
            else if (prefix.size() >= 2 && label.contains(prefix, Qt::CaseInsensitive))
                score = 1;
            else
                return;
        }
        items << Item{label, insert, kind, detail, score, order};
    };

    const QStringList names = tableNames();
    const SqlText::Segment statement = SqlText::statementAt(text, position);
    auto findTable = [&](const QString &name) -> const TableInfo * {
        for (const auto &t : m_schema)
            if (t.name.compare(name, Qt::CaseInsensitive) == 0)
                return &t;
        return nullptr;
    };

    if (!qualifier.isEmpty()) {
        const auto aliases = SqlText::aliases(statement.text, names);
        const TableInfo *table = findTable(aliases.value(qualifier.toLower(), qualifier));
        // In the table's own order, which is how people think of a row.
        if (table)
            for (int i = 0; i < table->columns.size(); ++i)
                add(table->columns[i].name, SqlText::identifier(table->columns[i].name), "column",
                    table->columns[i].type, i);
    } else {
        const QString keyword = SqlText::previousKeyword(text, start);
        const bool tableContext = QStringList{"FROM", "JOIN", "INTO", "UPDATE", "TABLE"}.contains(keyword);
        for (const auto &t : m_schema)
            add(t.name, SqlText::identifier(t.name), t.kind,
                t.kind == "view" ? tr("view") : t.rows >= 0 ? tr("%n rows", "", int(t.rows)) : QString(),
                tableContext ? 0 : 2);
        if (!tableContext) {
            const QStringList referenced = SqlText::referencedTables(statement.text, names);
            QSet<QString> seen;
            for (const auto &t : m_schema) {
                const bool inQuery = referenced.contains(t.name);
                if (!referenced.isEmpty() && !inQuery)
                    continue;
                for (const auto &c : t.columns) {
                    if (seen.contains(c.name.toLower()))
                        continue;
                    seen.insert(c.name.toLower());
                    add(c.name, SqlText::identifier(c.name), "column",
                        c.type.isEmpty() ? t.name : c.type + " · " + t.name, referenced.isEmpty() ? 1 : 0);
                }
            }
            const bool lower = !prefix.isEmpty() && prefix == prefix.toLower();
            for (const auto &k : SqlText::keywords())
                add(k, lower ? k.toLower() : k, "keyword", {}, 3);
            for (const auto &f : SqlText::functions())
                add(f, f + "(", "function", {}, 4);
        }
    }

    std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
        if (a.score != b.score)
            return a.score < b.score;
        if (a.order != b.order)
            return a.order < b.order;
        return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
    });
    // Nothing to offer when the only suggestion is what's already typed.
    if (!forced && items.size() == 1 && items.first().label.compare(prefix, Qt::CaseInsensitive) == 0)
        items.clear();

    QVariantList list;
    for (int i = 0; i < items.size() && i < 60; ++i)
        list << QVariantMap{{"label", items[i].label}, {"insert", items[i].insert},
                            {"kind", items[i].kind}, {"detail", items[i].detail}};
    out["items"] = list;
    return out;
}

QVariantMap Backend::statementAt(const QString &text, int position) const {
    const auto s = SqlText::statementAt(text, position);
    const int offset = s.blank ? s.start : int(text.indexOf(s.text, s.start));
    return {{"start", offset}, {"end", offset + s.text.size()}, {"text", s.blank ? QString() : s.text}};
}

bool Backend::isDestructive(const QString &sql) const { return SqlText::isDestructive(sql); }
QString Backend::identifier(const QString &name) const { return SqlText::identifier(name); }

void Backend::copy(const QString &text) {
    QGuiApplication::clipboard()->setText(text);
    setNotice(tr("Copied"));
}

namespace {

struct Format {
    QString name, extension;
};

Format formatInfo(const QString &format) {
    if (format == "json")
        return {"JSON", "json"};
    if (format == "markdown")
        return {"Markdown", "md"};
    if (format == "sql")
        return {"SQL", "sql"};
    return {"CSV", "csv"};
}

} // namespace

void Backend::copyResults(const QString &format) {
    if (!m_hasResults)
        return;
    QGuiApplication::clipboard()->setText(m_results.format(format, m_resultTable));
    setNotice(tr("Copied %n rows as %1", "", m_results.rowTotal()).arg(formatInfo(format).name));
}

void Backend::exportResults(const QString &format) {
    if (!m_picker || !m_hasResults)
        return;
    m_pending = Pending::Export;
    m_exportFormat = format;
    const Format info = formatInfo(format);
    QString base = !m_resultTable.isEmpty() ? m_resultTable
                 : isOpen() && m_db.path() != ":memory:" ? QFileInfo(m_db.path()).completeBaseName() : "results";
    m_picker->saveFile(tr("Export results as %1").arg(info.name), base + "." + info.extension,
                       tr("%1 files").arg(info.name), "*." + info.extension);
}

void Backend::clearNotice() { setNotice({}); }

QString Backend::draft() const {
    QFile file(draftPath(m_db.path()));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

void Backend::saveDraft(const QString &text) {
    if (!isOpen())
        return;
    const QString path = draftPath(m_db.path());
    if (text.trimmed().isEmpty()) {
        QFile::remove(path);
        return;
    }
    writeFile(path, text.toUtf8());
}

void Backend::setStatus(const QString &status, const QString &kind) {
    m_status = status;
    m_statusKind = kind;
    emit statusChanged();
}

void Backend::setNotice(const QString &notice) {
    if (notice == m_notice)
        return;
    m_notice = notice;
    emit noticeChanged();
}

void Backend::remember(const QString &sql) {
    for (int i = m_history.size() - 1; i >= 0; --i)
        if (m_history[i].toMap().value("sql").toString() == sql)
            m_history.removeAt(i);
    m_history.prepend(QVariantMap{{"sql", sql},
                                  {"at", QDateTime::currentDateTime().toString(Qt::ISODate)},
                                  {"database", databaseName()}});
    while (m_history.size() > HistoryLimit)
        m_history.removeLast();
    writeFile(statePath("history.json"), QJsonDocument(QJsonArray::fromVariantList(m_history)).toJson());
    emit historyChanged();
}

void Backend::rememberDatabase(const QString &path) {
    m_recent.removeAll(path);
    m_recent.prepend(path);
    while (m_recent.size() > RecentLimit)
        m_recent.removeLast();
    writeFile(statePath("recent.json"), QJsonDocument(QJsonArray::fromStringList(m_recent)).toJson());
    emit recentChanged();
}

QString Backend::draftPath(const QString &database) const {
    const QByteArray hash = QCryptographicHash::hash(database.toUtf8(), QCryptographicHash::Sha1).toHex();
    return statePath("drafts/" + QString::fromLatin1(hash) + ".sql");
}

QString Backend::statePath(const QString &name) const { return m_stateDirectory + "/" + name; }

// Written beside the target, then renamed over it, so a crash never leaves
// half a file.
bool Backend::writeFile(const QString &path, const QByteArray &data, QString *error) const {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}
