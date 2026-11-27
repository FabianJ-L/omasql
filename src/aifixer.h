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
    Q_PROPERTY(bool available READ available NOTIFY agentChanged)
    Q_PROPERTY(QString agent READ agent NOTIFY agentChanged)
    Q_PROPERTY(QString agentLabel READ agentLabel NOTIFY agentChanged)
    // The agents installed on this system, the chosen one among them.
    Q_PROPERTY(QStringList agents READ agents NOTIFY agentChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // The user agreed to send statements and the schema to this agent.
    Q_PROPERTY(bool allowed READ allowed NOTIFY allowedChanged)

public:
    struct Command {
        QString program;
        QStringList arguments;
        bool promptOnStdin = true;
        bool replyInFile = false; // the reply is written to <dir>/reply.txt
    };

    // agent overrides the system's default agent ("" asks the system, in the
    // background). Consent is remembered in stateDirectory ("" keeps it for
    // this run only).
    explicit AiFixer(Backend *backend, const QString &agent = QString(), QObject *parent = nullptr,
                     const QString &stateDirectory = QString());
    ~AiFixer() override;

    bool available() const { return !m_agent.isEmpty(); }
    QString agent() const { return m_agent; }
    QString agentLabel() const { return label(m_agent); }
    QStringList agents() const;
    // Picks the agent by hand; remembered in stateDirectory, and it wins over
    // the system's default agent from then on.
    Q_INVOKABLE void setAgent(const QString &agent);
    Q_INVOKABLE static QString label(const QString &agent);
    // Answers in the background (true), or opens in its own terminal.
    Q_INVOKABLE static bool answersInBackground(const QString &agent);
    bool busy() const { return m_process != nullptr; }
    bool allowed() const;
    Q_INVOKABLE void allow();

    // sql is the statement as it stands in the editor, error what SQLite said
    // about it, language the UI language for the one-line explanation.
    Q_INVOKABLE void fix(const QString &sql, const QString &error, const QString &language);
    Q_INVOKABLE void cancel();

    // Without asking omarchy-default-agent: OMASQL_AGENT or the first
    // headless agent installed.
    static QString fallbackAgent();
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
    void agentChanged();
    void allowedChanged();
    void fixed(const QString &original, const QString &sql, const QString &note);
    void failed(const QString &message);
    // An agent that needs a terminal was opened with the problem instead.
    void handedOver(const QString &agent);

private:
    void finished(int exitCode, QProcess::ExitStatus status);
    void launch(const Command &command, const QString &text, const QProcessEnvironment &env);
    void stop();
    QString consentPath() const;
    QString choicePath() const;

    QPointer<Backend> m_backend;
    QString m_agent;
    QProcess *m_process = nullptr;
    std::unique_ptr<QTemporaryDir> m_dir;
    QTimer m_timeout;
    QString m_original;
    QString m_stateDirectory;
    QStringList m_allowedThisRun;
    bool m_cancelled = false;
};
