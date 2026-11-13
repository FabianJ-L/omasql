#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

// Plain functions over SQL text: splitting into statements, finding the one
// under the cursor, and the context autocompletion needs. They know SQLite's
// quoting and comments but don't parse SQL, which keeps them fast enough to
// run on every keystroke.
namespace SqlText {

struct Segment {
    int start = 0; // first character of the segment
    int end = 0;   // one past its terminating semicolon, or the text's length
    QString text;  // trimmed, without the semicolon
    bool blank = true; // only whitespace and comments
};

QList<Segment> split(const QString &sql);
Segment statementAt(const QString &sql, int position);

// True inside a string literal or comment, where completion should stay out.
bool insideLiteral(const QString &sql, int position);

// The identifier being typed just before position, and where it starts.
QString wordBefore(const QString &sql, int position, int *start);

// "o" for "o.na|", the table or alias a dotted name hangs off.
QString qualifierBefore(const QString &sql, int wordStart);

// The last keyword before position, uppercased ("FROM", "WHERE"), skipping
// identifiers and commas so "FROM a, b|" still reports FROM.
QString previousKeyword(const QString &sql, int position);

// Known tables the statement names, and aliases ("o" -> "orders"), matched
// case-insensitively against the schema.
QStringList referencedTables(const QString &statement, const QStringList &tables);
QHash<QString, QString> aliases(const QString &statement, const QStringList &tables);

// Statements that can lose data without a second look: DROP, DELETE or
// UPDATE without WHERE, ALTER TABLE … DROP.
bool isDestructive(const QString &sql);

QString quoteIdentifier(const QString &name);
// The name as typed in SQL: bare when it can be, quoted otherwise.
QString identifier(const QString &name);

const QStringList &keywords();
const QStringList &functions();
bool isKeyword(const QString &word);

} // namespace SqlText
