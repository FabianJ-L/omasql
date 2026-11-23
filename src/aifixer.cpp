#include "aifixer.h"

#include "backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

#include <sqlite3.h>
#include <utility>

namespace {

// Long enough for a slow model on a big schema, short enough that a hung
// agent doesn't leave the button spinning forever.
const int TimeoutMs = 180000;
// The schema goes in whole unless it is huge; the statement's own tables
// are what matter, and they come first.
const int SchemaLimit = 40000;

QString findAgentProgram(const QString &agent) {
    // A user install in ~/.local/bin counts even when it isn't on PATH, as it
    // does for omarchy-agent.
    const QString user = QDir::homePath() + "/.local/bin/" + agent;
    if (QFileInfo(user).isExecutable())
        return user;
    return QStandardPaths::findExecutable(agent);
}

QString stripAnsi(QString text) {
    static const QRegularExpression ansi("\x1b\\[[0-9;?]*[ -/]*[@-~]|\x1b\\][^\x07]*\x07");
    return text.remove(ansi);
}

QString lastLine(const QString &text) {
    const QStringList lines = stripAnsi(text).split('\n', Qt::SkipEmptyParts);
    for (auto it = lines.rbegin(); it != lines.rend(); ++it)
        if (!it->trimmed().isEmpty())
            return it->trimmed().left(240);
    return {};
}

} // namespace

AiFixer::AiFixer(Backend *backend, const QString &agent, QObject *parent)
    : QObject(parent), m_backend(backend), m_agent(agent.isEmpty() ? defaultAgent() : agent) {
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(TimeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        stop();
        emit failed(tr("%1 took too long to answer").arg(agentLabel()));
    });
}

AiFixer::~AiFixer() {
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

// OMASQL_AGENT picks one by hand (a name, or the path of a program that reads
// the prompt on stdin and prints the reply); otherwise Omarchy's default
// agent; otherwise the first agent installed that can answer headless.
QString AiFixer::defaultAgent() {
    const QString forced = qEnvironmentVariable("OMASQL_AGENT").trimmed();
    if (!forced.isEmpty())
        return forced;
    if (!QStandardPaths::findExecutable("omarchy-default-agent").isEmpty()) {
        QProcess process;
        process.start("omarchy-default-agent", {});
        if (process.waitForFinished(3000) && process.exitCode() == 0) {
            const QString agent = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
            if (!agent.isEmpty())
                return agent;
        }
    }
    for (const char *agent : {"claude", "codex", "opencode"})
        if (!findAgentProgram(agent).isEmpty())
            return agent;
    return {};
}

QString AiFixer::agentLabel() const {
    if (m_agent.contains('/'))
        return tr("AI");
    static const QHash<QString, QString> names{
        {"claude", "Claude"}, {"codex", "Codex"}, {"opencode", "OpenCode"}, {"copilot", "Copilot"},
        {"cursor-agent", "Cursor"}, {"agy", "Antigravity"}, {"openclaw", "OpenClaw"}, {"qwen", "Qwen"},
        {"kimi", "Kimi"}, {"grok", "Grok"}, {"hermes", "Hermes"}, {"amp", "Amp"}, {"kilo", "Kilo"},
        {"devin", "Devin"}, {"crush", "Crush"}, {"muse", "Muse"}, {"pi", "Pi"}};
    if (names.contains(m_agent))
        return names.value(m_agent);
    return m_agent.isEmpty() ? tr("AI") : m_agent.left(1).toUpper() + m_agent.mid(1);
}

// One question, one answer, no tools: the agent only reads the prompt.
bool AiFixer::headlessCommand(const QString &agent, const QString &replyFile, Command *command) {
    if (agent.contains('/')) {
        if (!QFileInfo(agent).isExecutable())
            return false;
        *command = {agent, {}, true, false};
        return true;
    }
    const QString program = findAgentProgram(agent);
    if (program.isEmpty())
        return false;
    if (agent == "claude") {
        *command = {program, {"-p", "--output-format", "text", "--tools", "", "--no-session-persistence"}, true, false};
        return true;
    }
    if (agent == "codex") {
        *command = {program,
                    {"exec", "--skip-git-repo-check", "--ephemeral", "--sandbox", "read-only", "--color", "never",
                     "--output-last-message", replyFile, "-"},
                    true, true};
        return true;
    }
    return false;
}

QString AiFixer::prompt(const QString &sql, const QString &error, const QList<TableInfo> &schema,
                        const QString &language) {
    QString definitions;
    for (const auto &table : schema) {
        if (table.sql.isEmpty())
            continue;
        if (definitions.size() + table.sql.size() > SchemaLimit) {
            definitions += "-- (more tables left out)\n";
            break;
        }
        definitions += table.sql.trimmed() + ";\n";
    }
    if (definitions.isEmpty())
        definitions = "-- (the database has no tables yet)\n";

    QString reply = "English";
    const QLocale locale(language);
    if (!language.isEmpty() && locale.language() != QLocale::C && locale.language() != QLocale::English)
        reply = QLocale::languageToString(locale.language());

    return QStringLiteral(
               "You fix SQLite statements. This one failed in SQLite %1:\n\n"
               "```sql\n%2\n```\n\n"
               "SQLite said: %3\n\n"
               "The database schema:\n\n"
               "```sql\n%4```\n\n"
               "Reply with the corrected statement in a single ```sql code block, then one short sentence, "
               "in %5, saying what was wrong. Keep what the statement is meant to do, its layout, and its "
               "comments; change only what the fix needs. Do not use any tools and do not ask questions.")
        .arg(QString::fromLatin1(sqlite3_libversion()), sql.trimmed(), error.trimmed(), definitions, reply);
}

QString AiFixer::extractSql(const QString &rawReply, QString *note) {
    const QString reply = stripAnsi(rawReply).trimmed();
    static const QRegularExpression fence("```[ \\t]*(?:sql|sqlite|SQL)?[ \\t]*\\n(.*?)\\n?[ \\t]*```",
                                          QRegularExpression::DotMatchesEverythingOption);
    const auto match = fence.match(reply);
    if (!match.hasMatch()) {
        if (note)
            note->clear();
        return reply;
    }
    if (note) {
        QString after = reply.mid(match.capturedEnd()).trimmed();
        if (after.isEmpty())
            after = reply.left(match.capturedStart()).trimmed();
        *note = after.section('\n', 0, 0).trimmed().left(300);
    }
    return match.captured(1).trimmed();
}

QString AiFixer::fitInto(const QString &original, const QString &fixed) {
    int lead = 0;
    while (lead < original.size() && original.at(lead).isSpace())
        ++lead;
    int trail = original.size();
    while (trail > lead && original.at(trail - 1).isSpace())
        --trail;
    const bool semicolon = original.left(trail).endsWith(';');

    QString body = fixed.trimmed();
    while (body.endsWith(';'))
        body = body.left(body.size() - 1).trimmed();
    if (semicolon)
        body += ';';
    return original.left(lead) + body + original.mid(trail);
}

void AiFixer::fix(const QString &sql, const QString &error, const QString &language) {
    if (busy() || sql.trimmed().isEmpty())
        return;
    if (m_agent.isEmpty()) {
        emit failed(tr("No AI agent is set up. Choose one with: omarchy default agent <name>"));
        return;
    }

    const QList<TableInfo> schema = m_backend ? m_backend->schema() : QList<TableInfo>();
    const QString text = prompt(sql, error, schema, language);

    m_dir = std::make_unique<QTemporaryDir>();
    const QString replyFile = m_dir->isValid() ? m_dir->filePath("reply.txt") : QString();
    Command command;
    if (!headlessCommand(m_agent, replyFile, &command)) {
        // Agents that only work in a terminal get the problem as their first
        // prompt there; the user copies the fix back.
        m_dir.reset();
        const QString launcher = QStandardPaths::findExecutable("omarchy-agent-prompt");
        if (launcher.isEmpty() || !QProcess::startDetached(launcher, {text}, QDir::homePath())) {
            emit failed(tr("%1 can't be started from Omasql").arg(agentLabel()));
            return;
        }
        emit handedOver(agentLabel());
        return;
    }

    m_original = sql;
    m_cancelled = false;
    m_process = new QProcess(this);
    // Picks the account Omarchy's agent switcher selected, as omarchy-agent does.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString homeVariable = m_agent == "claude" ? "CLAUDE_CONFIG_DIR" : m_agent == "codex" ? "CODEX_HOME" : "";
    if (!homeVariable.isEmpty() && !env.contains(homeVariable)
        && !QStandardPaths::findExecutable("omarchy-agent-account-home").isEmpty()) {
        QProcess home;
        home.start("omarchy-agent-account-home", {m_agent});
        if (home.waitForFinished(3000) && home.exitCode() == 0) {
            const QString path = QString::fromUtf8(home.readAllStandardOutput()).trimmed();
            if (!path.isEmpty())
                env.insert(homeVariable, path);
        }
    }
    m_process->setProcessEnvironment(env);
    // Nowhere near a project, so the agent has no files to wander into.
    m_process->setWorkingDirectory(m_dir->isValid() ? m_dir->path() : QDir::tempPath());
    connect(m_process, &QProcess::finished, this, &AiFixer::finished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            stop();
            emit failed(tr("%1 can't be started from Omasql").arg(agentLabel()));
        }
    });
    m_process->start(command.program, command.arguments);
    if (!m_process)
        return; // failed to start, already reported
    if (command.promptOnStdin)
        m_process->write(text.toUtf8());
    m_process->closeWriteChannel();
    m_timeout.start();
    emit busyChanged();
}

void AiFixer::cancel() {
    if (!busy())
        return;
    m_cancelled = true;
    stop();
}

void AiFixer::stop() {
    m_timeout.stop();
    if (!m_process)
        return;
    QProcess *process = std::exchange(m_process, nullptr);
    process->disconnect(this);
    if (process->state() != QProcess::NotRunning) {
        process->kill();
        process->waitForFinished(1000);
    }
    process->deleteLater();
    m_dir.reset();
    emit busyChanged();
}

void AiFixer::finished(int exitCode, QProcess::ExitStatus status) {
    QString reply = QString::fromUtf8(m_process->readAllStandardOutput());
    const QString errors = QString::fromUtf8(m_process->readAllStandardError());
    if (m_dir && m_dir->isValid()) {
        QFile file(m_dir->filePath("reply.txt"));
        if (file.open(QIODevice::ReadOnly))
            reply = QString::fromUtf8(file.readAll());
    }
    const QString original = m_original;
    stop();

    if (status != QProcess::NormalExit || exitCode != 0) {
        const QString why = lastLine(errors).isEmpty() ? lastLine(reply) : lastLine(errors);
        emit failed(why.isEmpty() ? tr("%1 couldn't fix the statement").arg(agentLabel())
                                  : tr("%1 couldn't fix the statement: %2").arg(agentLabel(), why));
        return;
    }
    QString note;
    const QString sql = extractSql(reply, &note);
    if (sql.isEmpty()) {
        emit failed(tr("%1 answered without a statement").arg(agentLabel()));
        return;
    }
    emit fixed(original, fitInto(original, sql), note);
}
