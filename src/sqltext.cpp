#include "sqltext.h"

#include <QRegularExpression>
#include <QSet>

namespace SqlText {

namespace {

bool isIdentifierStart(QChar c) { return c.isLetter() || c == '_'; }
bool isIdentifierPart(QChar c) { return c.isLetterOrNumber() || c == '_' || c == '$'; }

// Where the literal or comment starting at i ends (one past it), and whether
// it was closed. Returns -1 when no literal starts at i.
int literalEnd(const QString &sql, int i, bool *closed) {
    const int n = sql.size();
    const QChar c = sql[i];
    *closed = true;
    if (c == '-' && i + 1 < n && sql[i + 1] == '-') {
        const int e = sql.indexOf('\n', i);
        if (e < 0)
            *closed = false;
        return e < 0 ? n : e;
    }
    if (c == '/' && i + 1 < n && sql[i + 1] == '*') {
        const int e = sql.indexOf("*/", i + 2);
        if (e < 0)
            *closed = false;
        return e < 0 ? n : e + 2;
    }
    if (c == '\'' || c == '"' || c == '`') {
        int j = i + 1;
        while (j < n) {
            if (sql[j] == c) {
                if (j + 1 < n && sql[j + 1] == c) {
                    j += 2;
                    continue;
                }
                return j + 1;
            }
            ++j;
        }
        *closed = false;
        return n;
    }
    if (c == '[') {
        const int e = sql.indexOf(']', i + 1);
        if (e < 0)
            *closed = false;
        return e < 0 ? n : e + 1;
    }
    return -1;
}

bool isComment(const QString &sql, int i) {
    return (sql[i] == '-' || sql[i] == '/') && i + 1 < sql.size() && sql[i + 1] == (sql[i] == '-' ? '-' : '*');
}

QString unquote(QString name) {
    if (name.size() >= 2) {
        const QChar f = name.front(), b = name.back();
        if ((f == '"' && b == '"') || (f == '`' && b == '`'))
            return name.mid(1, name.size() - 2).replace(QString(2, f), QString(f));
        if (f == '[' && b == ']')
            return name.mid(1, name.size() - 2);
    }
    return name;
}

QString findTable(const QString &name, const QStringList &tables) {
    for (const auto &table : tables)
        if (table.compare(name, Qt::CaseInsensitive) == 0)
            return table;
    return {};
}

// The text with comments and string literals blanked out, same length, so
// keyword checks don't trip over 'DROP' inside a string.
QString codeOnly(const QString &sql) {
    QString out = sql;
    for (int i = 0; i < sql.size();) {
        bool closed;
        const int e = literalEnd(sql, i, &closed);
        if (e < 0 || sql[i] == '"' || sql[i] == '`' || sql[i] == '[') {
            i = e < 0 ? i + 1 : e;
            continue;
        }
        for (int j = i; j < e; ++j)
            out[j] = ' ';
        i = e;
    }
    return out;
}

} // namespace

QList<Segment> split(const QString &sql) {
    QList<Segment> out;
    const int n = sql.size();
    int segmentStart = 0;
    bool blank = true;
    QStringList lead;
    QString lastWord;

    auto finish = [&](int end, int contentEnd) {
        Segment s;
        s.start = segmentStart;
        s.end = end;
        s.text = sql.mid(segmentStart, contentEnd - segmentStart).trimmed();
        s.blank = blank;
        out << s;
        segmentStart = end;
        blank = true;
        lead.clear();
        lastWord.clear();
    };

    for (int i = 0; i < n;) {
        const QChar c = sql[i];
        bool closed;
        const int e = literalEnd(sql, i, &closed);
        if (e >= 0) {
            if (!isComment(sql, i)) {
                blank = false;
                lastWord.clear();
            }
            i = e;
            continue;
        }
        if (isIdentifierStart(c)) {
            const int s = i;
            while (i < n && isIdentifierPart(sql[i]))
                ++i;
            lastWord = sql.mid(s, i - s).toUpper();
            if (lead.size() < 3)
                lead << lastWord;
            blank = false;
            continue;
        }
        if (c == ';') {
            // A trigger body holds its own semicolons and ends with END;.
            const bool trigger = lead.value(0) == "CREATE"
                && (lead.value(1) == "TRIGGER"
                    || ((lead.value(1) == "TEMP" || lead.value(1) == "TEMPORARY") && lead.value(2) == "TRIGGER"));
            if (!trigger || lastWord == "END") {
                finish(i + 1, i);
                ++i;
                continue;
            }
        }
        if (!c.isSpace())
            blank = false;
        ++i;
    }
    if (segmentStart < n || out.isEmpty())
        finish(n, n);
    return out;
}

Segment statementAt(const QString &sql, int position) {
    const auto segments = split(sql);
    int index = segments.size() - 1;
    for (int i = 0; i < segments.size(); ++i) {
        if (position >= segments[i].start && position < segments[i].end) {
            index = i;
            break;
        }
    }
    // Right after a semicolon on the same line still means that statement.
    if (index > 0 && position >= segments[index].start) {
        const QString before = sql.mid(segments[index].start, position - segments[index].start);
        if (!before.contains('\n') && before.trimmed().isEmpty())
            --index;
    }
    while (index > 0 && segments[index].blank)
        --index;
    if (segments[index].blank) {
        for (const auto &s : segments)
            if (!s.blank)
                return s;
    }
    return segments[index];
}

bool insideLiteral(const QString &sql, int position) {
    for (int i = 0; i < sql.size() && i < position;) {
        bool closed;
        const int e = literalEnd(sql, i, &closed);
        if (e < 0) {
            ++i;
            continue;
        }
        const bool lineComment = sql[i] == '-';
        if (position > i && (position < e || (position == e && (!closed || lineComment))))
            return true;
        i = e;
    }
    return false;
}

QString wordBefore(const QString &sql, int position, int *start) {
    int i = qBound(0, position, int(sql.size()));
    while (i > 0 && isIdentifierPart(sql[i - 1]))
        --i;
    if (start)
        *start = i;
    return sql.mid(i, position - i);
}

QString qualifierBefore(const QString &sql, int wordStart) {
    if (wordStart <= 0 || sql[wordStart - 1] != '.')
        return {};
    int end = wordStart - 1;
    if (end > 0 && (sql[end - 1] == '"' || sql[end - 1] == '`' || sql[end - 1] == ']')) {
        const QChar close = sql[end - 1];
        const QChar open = close == ']' ? QChar('[') : close;
        const int s = sql.lastIndexOf(open, end - 2);
        return s < 0 ? QString() : unquote(sql.mid(s, end - s));
    }
    int s = end;
    while (s > 0 && isIdentifierPart(sql[s - 1]))
        --s;
    return sql.mid(s, end - s);
}

QString previousKeyword(const QString &sql, int position) {
    QString keyword;
    bool afterValue = false;
    const int n = qMin(position, int(sql.size()));
    for (int i = 0; i < n;) {
        const QChar c = sql[i];
        bool closed;
        const int e = literalEnd(sql, i, &closed);
        if (e >= 0) {
            if (!isComment(sql, i))
                afterValue = true;
            i = e;
            continue;
        }
        if (isIdentifierStart(c) || c.isDigit()) {
            const int s = i;
            while (i < n && isIdentifierPart(sql[i]))
                ++i;
            const QString word = sql.mid(s, i - s).toUpper();
            if (!c.isDigit() && isKeyword(word)) {
                keyword = word;
                afterValue = false;
            } else {
                afterValue = true;
            }
            continue;
        }
        if (c == ';') {
            keyword.clear();
            afterValue = false;
        } else if (c == '.') {
            // "a.b" is still one name.
        } else if (!c.isSpace() && c != ')') {
            afterValue = false;
        }
        ++i;
    }
    return afterValue ? QString() : keyword;
}

static const QRegularExpression &referenceExpression() {
    static const QRegularExpression expression(
        // The alias may not be a keyword, or "a, b FROM t" would read FROM as b's
        // alias and swallow the table after it.
        R"re((?:\bFROM|\bJOIN|\bUPDATE|\bINTO|,)\s+("(?:[^"]|"")+"|`[^`]+`|\[[^\]]+\]|[\w$]+))re"
        R"re((?:\s+(?:AS\s+)?(?!(?:FROM|WHERE|JOIN|ON|USING|GROUP|ORDER|LIMIT|SET|VALUES|SELECT|LEFT|RIGHT|INNER|OUTER|CROSS|NATURAL|FULL|UNION|EXCEPT|INTERSECT|HAVING|WINDOW|RETURNING|DEFAULT)\b)([\w$]+))?)re",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return expression;
}

QStringList referencedTables(const QString &statement, const QStringList &tables) {
    QStringList out;
    auto it = referenceExpression().globalMatch(statement);
    while (it.hasNext()) {
        const QString table = findTable(unquote(it.next().captured(1)), tables);
        if (!table.isEmpty() && !out.contains(table))
            out << table;
    }
    return out;
}

QHash<QString, QString> aliases(const QString &statement, const QStringList &tables) {
    QHash<QString, QString> out;
    auto it = referenceExpression().globalMatch(statement);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString table = findTable(unquote(match.captured(1)), tables);
        if (table.isEmpty())
            continue;
        out.insert(table.toLower(), table);
        const QString alias = match.captured(2);
        if (!alias.isEmpty() && !isKeyword(alias.toUpper()))
            out.insert(alias.toLower(), table);
    }
    return out;
}

bool isDestructive(const QString &sql) {
    for (const auto &segment : split(codeOnly(sql))) {
        const QString text = segment.text.toUpper();
        static const QRegularExpression words(R"(\b[A-Z_]+\b)");
        QStringList tokens;
        auto it = words.globalMatch(text);
        while (it.hasNext())
            tokens << it.next().captured();
        if (tokens.isEmpty())
            continue;
        const QString first = tokens.first();
        if (first == "DROP")
            return true;
        if ((first == "DELETE" || first == "UPDATE") && !tokens.contains("WHERE"))
            return true;
        if (first == "ALTER" && tokens.contains("DROP"))
            return true;
    }
    return false;
}

QString quoteIdentifier(const QString &name) {
    return '"' + QString(name).replace('"', "\"\"") + '"';
}

QString identifier(const QString &name) {
    static const QRegularExpression bare("^[A-Za-z_][A-Za-z0-9_]*$");
    if (bare.match(name).hasMatch() && !isKeyword(name.toUpper()))
        return name;
    return quoteIdentifier(name);
}

const QStringList &keywords() {
    static const QStringList list = QString(
        "ABORT ACTION ADD AFTER ALL ALTER ALWAYS ANALYZE AND AS ASC ATTACH AUTOINCREMENT BEFORE BEGIN BETWEEN "
        "BY CASCADE CASE CAST CHECK COLLATE COLUMN COMMIT CONFLICT CONSTRAINT CREATE CROSS CURRENT CURRENT_DATE "
        "CURRENT_TIME CURRENT_TIMESTAMP DATABASE DEFAULT DEFERRABLE DEFERRED DELETE DESC DETACH DISTINCT DO DROP "
        "EACH ELSE END ESCAPE EXCEPT EXCLUDE EXCLUSIVE EXISTS EXPLAIN FAIL FALSE FILTER FIRST FOLLOWING FOR FOREIGN "
        "FROM FULL GENERATED GLOB GROUP GROUPS HAVING IF IGNORE IMMEDIATE IN INDEX INDEXED INITIALLY INNER INSERT "
        "INSTEAD INTERSECT INTO IS ISNULL JOIN KEY LAST LEFT LIKE LIMIT MATCH MATERIALIZED NATURAL NO NOT NOTHING "
        "NOTNULL NULL NULLS OF OFFSET ON OR ORDER OTHERS OUTER OVER PARTITION PLAN PRAGMA PRECEDING PRIMARY QUERY "
        "RAISE RANGE RECURSIVE REFERENCES REGEXP REINDEX RELEASE RENAME REPLACE RESTRICT RETURNING RIGHT ROLLBACK "
        "ROW ROWS SAVEPOINT SELECT SET STRICT TABLE TEMP TEMPORARY THEN TIES TO TRANSACTION TRIGGER TRUE UNBOUNDED "
        "UNION UNIQUE UPDATE USING VACUUM VALUES VIEW VIRTUAL WHEN WHERE WINDOW WITH WITHOUT "
        "INTEGER TEXT REAL BLOB NUMERIC ANY").split(' ');
    return list;
}

bool isKeyword(const QString &word) {
    static const QSet<QString> set(keywords().begin(), keywords().end());
    return set.contains(word);
}

const QStringList &functions() {
    static const QStringList list = QString(
        "abs changes char coalesce concat concat_ws format glob hex ifnull iif instr last_insert_rowid length "
        "likelihood likely lower ltrim max min nullif octet_length printf quote random randomblob replace round "
        "rtrim sign soundex sqlite_version substr substring total_changes trim typeof unhex unicode unlikely upper "
        "zeroblob avg count group_concat string_agg sum total date time datetime julianday unixepoch strftime "
        "timediff json json_array json_array_length json_extract json_insert json_object json_patch json_remove "
        "json_replace json_set json_type json_valid json_quote json_group_array json_group_object json_each "
        "json_tree row_number rank dense_rank percent_rank cume_dist ntile lag lead first_value last_value "
        "nth_value acos asin atan atan2 ceil ceiling cos degrees exp floor ln log log10 log2 mod pi pow power "
        "radians sin sqrt tan trunc").split(' ');
    return list;
}

} // namespace SqlText
