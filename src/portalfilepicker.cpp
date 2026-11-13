// The portal request flow follows Omacut's portalfilepicker.cpp
// (MIT, David Heinemeier Hansson).
#include "portalfilepicker.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRandomGenerator>
#include <QUrl>

namespace {

const char *Service = "org.freedesktop.portal.Desktop";
const char *ObjectPath = "/org/freedesktop/portal/desktop";
const char *Interface = "org.freedesktop.portal.FileChooser";

struct FilterRule {
    uint type = 0; // 0 is a glob pattern, 1 a MIME type
    QString pattern;
};
struct Filter {
    QString name;
    QList<FilterRule> rules;
};

} // namespace

Q_DECLARE_METATYPE(FilterRule)
Q_DECLARE_METATYPE(Filter)

QDBusArgument &operator<<(QDBusArgument &argument, const FilterRule &rule) {
    argument.beginStructure();
    argument << rule.type << rule.pattern;
    argument.endStructure();
    return argument;
}
const QDBusArgument &operator>>(const QDBusArgument &argument, FilterRule &rule) {
    argument.beginStructure();
    argument >> rule.type >> rule.pattern;
    argument.endStructure();
    return argument;
}
QDBusArgument &operator<<(QDBusArgument &argument, const Filter &filter) {
    argument.beginStructure();
    argument << filter.name << filter.rules;
    argument.endStructure();
    return argument;
}
const QDBusArgument &operator>>(const QDBusArgument &argument, Filter &filter) {
    argument.beginStructure();
    argument >> filter.name >> filter.rules;
    argument.endStructure();
    return argument;
}

PortalFilePicker::PortalFilePicker(QObject *parent) : FilePicker(parent) {
    qDBusRegisterMetaType<FilterRule>();
    qDBusRegisterMetaType<QList<FilterRule>>();
    qDBusRegisterMetaType<Filter>();
    qDBusRegisterMetaType<QList<Filter>>();
}

void PortalFilePicker::openDatabase(const QString &title) {
    const Filter databases{tr("SQLite databases"),
                           {{0, "*.db"}, {0, "*.sqlite"}, {0, "*.sqlite3"}, {0, "*.db3"},
                            {1, "application/vnd.sqlite3"}, {1, "application/x-sqlite3"}}};
    const Filter all{tr("All files"), {{0, "*"}}};
    QVariantMap options;
    options["filters"] = QVariant::fromValue(QList<Filter>{databases, all});
    options["modal"] = true;
    request("OpenFile", title, options);
}

void PortalFilePicker::saveCsv(const QString &title, const QString &suggestedName) {
    QVariantMap options;
    options["current_name"] = suggestedName;
    options["filters"] = QVariant::fromValue(QList<Filter>{{tr("CSV files"), {{0, "*.csv"}}}});
    options["modal"] = true;
    request("SaveFile", title, options);
}

void PortalFilePicker::request(const QString &method, const QString &title, QVariantMap options) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        emit failed(tr("Can't reach the D-Bus session bus, so no file dialog can open."));
        return;
    }
    stopListening();

    // Subscribe to the Response before calling, at the path the portal will
    // use for this token, so a fast answer isn't missed.
    const QString token = QString("lode%1_%2").arg(++m_counter).arg(QRandomGenerator::global()->generate());
    options["handle_token"] = token;
    const QString sender = bus.baseService().mid(1).replace('.', '_');
    listen(QString("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token));

    QDBusMessage message = QDBusMessage::createMethodCall(Service, ObjectPath, Interface, method);
    message << QString() << title << options;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *call;
        if (reply.isError()) {
            stopListening();
            emit failed(tr("The file dialog didn't open: %1").arg(reply.error().message()));
            return;
        }
        // Older portals ignore the token; follow the path they return.
        if (reply.value().path() != m_requestPath)
            listen(reply.value().path());
    });
}

void PortalFilePicker::listen(const QString &path) {
    stopListening();
    m_requestPath = path;
    QDBusConnection::sessionBus().connect(Service, path, "org.freedesktop.portal.Request", "Response", this,
                                          SLOT(onResponse(uint, QVariantMap)));
}

void PortalFilePicker::stopListening() {
    if (m_requestPath.isEmpty())
        return;
    QDBusConnection::sessionBus().disconnect(Service, m_requestPath, "org.freedesktop.portal.Request", "Response",
                                             this, SLOT(onResponse(uint, QVariantMap)));
    m_requestPath.clear();
}

void PortalFilePicker::onResponse(uint response, const QVariantMap &results) {
    stopListening();
    if (response != 0) {
        emit cancelled();
        return;
    }
    const QStringList uris = results.value("uris").toStringList();
    const QString path = uris.isEmpty() ? QString() : QUrl(uris.first()).toLocalFile();
    if (path.isEmpty())
        emit failed(tr("The file dialog returned no local file."));
    else
        emit picked(path);
}
