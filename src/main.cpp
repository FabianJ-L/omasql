// Omasql — a light SQLite client: write queries with completion, browse the
// schema, and read results without leaving the keyboard.

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>
#include <QtQml>

#include "aifixer.h"
#include "backend.h"
#include "highlighter.h"
#include "portalfilepicker.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("omasql");
    app.setApplicationVersion("0.3.1");

    // Matches the window to omasql.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("omasql");
    app.setWindowIcon(QIcon::fromTheme("omasql"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "A light SQLite client."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("database", QCoreApplication::translate("main", "The SQLite file to open."));
    parser.process(app);

    qmlRegisterType<SqlHighlighter>("Omasql", 1, 0, "SqlHighlighter");

    Theme theme;
    PortalFilePicker picker;
    Backend backend(&picker);
    AiFixer ai(&backend, {}, nullptr, backend.stateDirectory());
    if (!parser.positionalArguments().isEmpty())
        backend.openDatabase(parser.positionalArguments().first());

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("ai", &ai);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
