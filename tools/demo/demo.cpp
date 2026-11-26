// Records docs/demo.gif: runs the real window offscreen against a sample shop
// database, types a query with a mistake, lets "Fix with AI" correct it, and
// runs it. Frames are grabbed from the window, so nothing touches the desktop
// and every run looks the same. The agent is a stub with a fixed reply, so the
// recording needs no account and no network. Run it with bin/demo.

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

#include "aifixer.h"
#include "backend.h"
#include "highlighter.h"
#include "theme.h"
#include "translator.h"

namespace {

constexpr int FrameMs = 80;

const char *Schema = R"(
CREATE TABLE customers (id INTEGER PRIMARY KEY, name TEXT NOT NULL, city TEXT);
CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT NOT NULL, price REAL NOT NULL);
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    customer_id INTEGER NOT NULL REFERENCES customers(id),
    product_id INTEGER NOT NULL REFERENCES products(id),
    qty INTEGER NOT NULL,
    placed_at TEXT NOT NULL
);
CREATE INDEX orders_customer ON orders(customer_id);
INSERT INTO customers (name, city) VALUES
    ('Ada Lovelace', 'London'), ('Grace Hopper', 'New York'), ('Linus Torvalds', 'Portland'),
    ('Margaret Hamilton', 'Boston'), ('Ken Thompson', 'Berkeley'), ('Barbara Liskov', 'Cambridge'),
    ('Dennis Ritchie', 'Murray Hill'), ('Frances Allen', 'Peru, NY'), ('Edsger Dijkstra', 'Austin'),
    ('Radia Perlman', 'Seattle');
INSERT INTO products (name, price) VALUES
    ('Mechanical keyboard', 129.0), ('Trackball', 69.5), ('USB-C dock', 189.0), ('27" monitor', 329.0),
    ('Desk lamp', 45.0), ('Headphones', 99.0), ('Webcam', 74.0), ('Laptop stand', 39.9);
-- A small deterministic generator, so every recording shows the same rows.
WITH RECURSIVE n(i, s) AS (
    SELECT 1, 42 UNION ALL SELECT i + 1, (s * 1103515245 + 12345) % 2147483648 FROM n WHERE i < 240)
INSERT INTO orders (customer_id, product_id, qty, placed_at)
SELECT 1 + (s / 7) % 10, 1 + (s / 70) % 8, 1 + (s / 700) % 3, date('2026-01-01', '+' || ((s / 2100) % 330) || ' days')
FROM n;
)";

const char *Query = "SELECT c.name, c.city, sum(o.qty * p.price) AS spent\n"
                    "FROM orders o\n"
                    "JOIN customers c ON c.id = o.customer_id\n"
                    "JOIN products p ON p.id = o.product_id\n"
                    "GROUP BY c.nme\n"
                    "ORDER BY spent DESC;";

const char *Reply = "```sql\n"
                    "SELECT c.name, c.city, sum(o.qty * p.price) AS spent\n"
                    "FROM orders o\n"
                    "JOIN customers c ON c.id = o.customer_id\n"
                    "JOIN products p ON p.id = o.product_id\n"
                    "GROUP BY c.id\n"
                    "ORDER BY spent DESC\n"
                    "```\n"
                    "customers has no column nme; grouping by c.id keeps customers with the same name apart.\n";

class Recorder {
public:
    Recorder(QQuickWindow *window, const QString &directory) : m_window(window), m_directory(directory) {}

    // Lets the window run for ms, grabbing a frame every FrameMs.
    void hold(int ms) {
        for (int elapsed = 0; elapsed < ms; elapsed += FrameMs)
            frame();
    }

    // Holds until done() is true (at most timeoutMs), then for ms more.
    template <typename Done> void until(Done done, int ms, int timeoutMs = 15000) {
        QElapsedTimer timer;
        timer.start();
        while (!done() && timer.elapsed() < timeoutMs)
            frame();
        hold(ms);
    }

    void type(const QString &text) {
        for (const QChar c : text) {
            if (c == '\n')
                QTest::keyClick(m_window, Qt::Key_Return);
            else
                QTest::keyClick(m_window, c.toLatin1());
            // Two characters a frame reads as quick, steady typing.
            if (++m_typed % 2 == 0)
                frame();
        }
        frame();
    }

    void key(Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QTest::keyClick(m_window, key, modifiers);
    }

    int frames() const { return m_frames; }

private:
    void frame() {
        QElapsedTimer timer;
        timer.start();
        QCoreApplication::processEvents(QEventLoop::AllEvents, FrameMs);
        m_window->grabWindow().save(QString("%1/%2.png").arg(m_directory).arg(m_frames++, 5, 10, QChar('0')));
        const qint64 left = FrameMs - timer.elapsed();
        if (left > 0)
            QTest::qWait(int(left));
    }

    QQuickWindow *m_window;
    QString m_directory;
    int m_frames = 0;
    int m_typed = 0;
};

} // namespace

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    if (app.arguments().size() < 2) {
        qWarning("usage: omasql_demo <frame directory>");
        return 2;
    }
    const QString frames = app.arguments().at(1);
    QDir().mkpath(frames);

    QTemporaryDir work;
    const QString agent = work.filePath("agent");
    {
        QFile reply(work.filePath("reply.txt"));
        if (!reply.open(QIODevice::WriteOnly))
            return 1;
        reply.write(Reply);
        QFile script(agent);
        if (!script.open(QIODevice::WriteOnly))
            return 1;
        script.write(QString("#!/bin/sh\ncat > /dev/null\nsleep 1.6\ncat '%1'\n").arg(reply.fileName()).toUtf8());
        script.setPermissions(script.permissions() | QFileDevice::ExeOwner);
    }
    // A fixed, short path, since the top bar shows it.
    const QString shop = QDir::tempPath() + "/omasql-demo";
    QDir(shop).removeRecursively();
    QDir().mkpath(shop);
    const QString database = shop + "/shop.db";
    {
        Database db;
        QString error;
        if (db.open(database, &error)) {
            const QueryResult created = db.execute(Schema, 10);
            error = created.ok ? QString() : created.error;
        }
        if (!error.isEmpty()) {
            qWarning("couldn't create the sample database: %s", qPrintable(error));
            return 1;
        }
    }

    QQuickStyle::setStyle("Material");
    // No language file: the recording is in English whatever this system uses.
    Translator translator(work.filePath("no-language"));
    app.installTranslator(&translator);
    qmlRegisterType<SqlHighlighter>("Omasql", 1, 0, "SqlHighlighter");

    Theme theme;
    Backend backend(nullptr, work.filePath("state"));
    AiFixer ai(&backend, agent, nullptr, work.filePath("state"));
    ai.allow();
    backend.openDatabase(database);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("ai", &ai);
    engine.load(QUrl("qrc:/Main.qml"));
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 1;
    window->resize(1200, 720);
    window->requestActivate();
    QTest::qWait(300);

    Recorder r(window, frames);
    r.hold(900);
    r.type(Query);
    r.hold(500);

    // The mistake: SQLite points at it, and the fix is one key away.
    r.key(Qt::Key_Return, Qt::ControlModifier);
    r.until([&] { return !backend.running(); }, 1800);
    if (backend.lastError().isEmpty())
        qWarning("the query was meant to fail");

    r.key(Qt::Key_Period, Qt::ControlModifier);
    r.until([&] { return !ai.busy(); }, 2400);

    r.key(Qt::Key_Return, Qt::ControlModifier);
    r.until([&] { return !backend.running(); }, 1400);
    if (!backend.lastError().isEmpty())
        qWarning("the fixed query failed: %s", qPrintable(backend.lastError()));

    // Walk the results; the row inspector follows.
    r.key(Qt::Key_3, Qt::ControlModifier);
    r.hold(300);
    for (int i = 0; i < 3; ++i) {
        r.key(Qt::Key_Down);
        r.hold(400);
    }
    r.hold(2600);

    qInfo("%d frames in %s", r.frames(), qPrintable(frames));
    QDir(shop).removeRecursively();
    return 0;
}
