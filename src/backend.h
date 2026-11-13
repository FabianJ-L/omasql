#pragma once

#include "database.h"
#include "resultmodel.h"

#include <QFutureWatcher>
#include <QObject>
#include <QVariantList>

class FilePicker;

// The open database, the query that runs against it, and what the window
// shows about both. Queries run on a worker thread so the window stays
// responsive and Esc can interrupt them.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY databaseChanged)
    Q_PROPERTY(QString databasePath READ databasePath NOTIFY databaseChanged)
    Q_PROPERTY(QString databaseName READ databaseName NOTIFY databaseChanged)
    Q_PROPERTY(QVariantList tables READ tables NOTIFY schemaChanged)
    Q_PROPERTY(QStringList tableNames READ tableNames NOTIFY schemaChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(ResultModel *results READ results CONSTANT)
    Q_PROPERTY(bool hasResults READ hasResults NOTIFY statusChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusKind READ statusKind NOTIFY statusChanged)
    Q_PROPERTY(QString lastSql READ lastSql NOTIFY statusChanged)
    Q_PROPERTY(QVariantList recent READ recent NOTIFY recentChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)

public:
    static constexpr int MaxRows = 20000;

    explicit Backend(FilePicker *picker, const QString &stateDirectory = {}, QObject *parent = nullptr);
    ~Backend() override;

    bool isOpen() const { return m_db.isOpen(); }
    QString databasePath() const { return m_db.path(); }
    QString databaseName() const;
    QVariantList tables() const;
    QStringList tableNames() const;
    bool running() const { return m_watcher.isRunning(); }
    ResultModel *results() { return &m_results; }
    bool hasResults() const { return m_hasResults; }
    QString status() const { return m_status; }
    QString statusKind() const { return m_statusKind; }
    QString lastSql() const { return m_lastSql; }
    QVariantList recent() const;
    QVariantList history() const { return m_history; }
    QString notice() const { return m_notice; }
    const QList<TableInfo> &schema() const { return m_schema; }

    Q_INVOKABLE bool openDatabase(const QString &path);
    Q_INVOKABLE void chooseDatabase();
    Q_INVOKABLE void openScratch();
    Q_INVOKABLE void closeDatabase();
    Q_INVOKABLE void forgetRecent(const QString &path);
    Q_INVOKABLE void refreshSchema();

    Q_INVOKABLE void run(const QString &sql);
    Q_INVOKABLE void preview(const QString &table);
    Q_INVOKABLE void cancel();
    void waitForIdle();

    Q_INVOKABLE QVariantMap completions(const QString &text, int position, bool forced) const;
    Q_INVOKABLE QVariantMap statementAt(const QString &text, int position) const;
    Q_INVOKABLE bool isDestructive(const QString &sql) const;
    Q_INVOKABLE QString identifier(const QString &name) const;

    Q_INVOKABLE void copy(const QString &text);
    Q_INVOKABLE void copyResultsCsv();
    Q_INVOKABLE void exportCsv();
    Q_INVOKABLE void clearNotice();

    Q_INVOKABLE QString draft() const;
    Q_INVOKABLE void saveDraft(const QString &text);

signals:
    void databaseChanged();
    void schemaChanged();
    void runningChanged();
    void statusChanged();
    void recentChanged();
    void historyChanged();
    void noticeChanged();
    void queryFinished(bool ok, int errorOffset);

private:
    void start(const QString &sql, bool remember);
    void finished();
    void setStatus(const QString &status, const QString &kind);
    void setNotice(const QString &notice);
    void remember(const QString &sql);
    void rememberDatabase(const QString &path);
    QString draftPath(const QString &database) const;
    QString statePath(const QString &name) const;
    bool writeFile(const QString &path, const QByteArray &data, QString *error = nullptr) const;

    FilePicker *m_picker;
    QString m_stateDirectory;
    Database m_db;
    QList<TableInfo> m_schema;
    ResultModel m_results;
    QFutureWatcher<QueryResult> m_watcher;
    bool m_hasResults = false;
    QString m_status, m_statusKind, m_lastSql, m_notice;
    QStringList m_recent;
    QVariantList m_history;
    enum class Pending { None, Open, Export } m_pending = Pending::None;
};
