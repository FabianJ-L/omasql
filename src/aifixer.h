#pragma once

#include "database.h"

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>

#include <memory>

class Backend;

// Asks the coding agent this system is set up with (Omarchy's default agent)
// to correct a statement SQLite rejected. Agents that can answer without a
// terminal run in the background and the fix comes back to the editor; any
// other agent opens in its own window with the problem as its first prompt.
class AiFixer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString agent READ agent CONSTANT)
    Q_PROPERTY(QString agentLabel READ agentLabel CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    struct Command {
        QString program;
        QStringList arguments;
        bool promptOnStdin = true;
        bool replyInFile = false; // the reply is written to <dir>/reply.txt
    };

    // agent overrides the system's default agent ("" asks the system).
    explicit AiFixer(Backend *backend, const QString &agent = QString(), QObject *parent = nullptr);
    ~AiFixer() override;

    bool available() const { return !m_agent.isEmpty(); }
    QString agent() const { return m_agent; }
    QString agentLabel() const;
    bool busy() const { return m_process != nullptr; }

    // sql is the statement as it stands in the editor, error what SQLite said
    // about it, language the UI language for the one-line explanation.
    Q_INVOKABLE void fix(const QString &sql, const QString &error, const QString &language);
    Q_INVOKABLE void cancel();

    static QString defaultAgent();
    static bool headlessCommand(const QString &agent, const QString &replyFile, Command *command);
    static QString prompt(const QString &sql, const QString &error, const QList<TableInfo> &schema,
                          const QString &language);
    // The SQL in the agent's reply, and the sentence after it in *note.
    static QString extractSql(const QString &reply, QString *note = nullptr);
    // fixed, wearing original's surrounding whitespace and trailing semicolon,
    // so it drops into the editor where original was.
    static QString fitInto(const QString &original, const QString &fixed);

signals:
    void busyChanged();
    void fixed(const QString &original, const QString &sql, const QString &note);
    void failed(const QString &message);
    // An agent that needs a terminal was opened with the problem instead.
    void handedOver(const QString &agent);

private:
    void finished(int exitCode, QProcess::ExitStatus status);
    void stop();

    QPointer<Backend> m_backend;
    QString m_agent;
    QProcess *m_process = nullptr;
    std::unique_ptr<QTemporaryDir> m_dir;
    QTimer m_timeout;
    QString m_original;
    bool m_cancelled = false;
};
