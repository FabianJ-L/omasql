#include "highlighter.h"

#include "sqltext.h"

#include <QTextCharFormat>

namespace {
enum State { Normal = -1, InComment = 1, InString = 2, InQuoted = 3 };

QTextCharFormat styled(const QColor &color, bool bold = false, bool italic = false) {
    QTextCharFormat f;
    if (color.isValid())
        f.setForeground(color);
    if (bold)
        f.setFontWeight(QFont::DemiBold);
    f.setFontItalic(italic);
    return f;
}
} // namespace

SqlHighlighter::SqlHighlighter(QObject *parent) : QSyntaxHighlighter(parent) {
    connect(this, &SqlHighlighter::changed, this, &QSyntaxHighlighter::rehighlight);
}

void SqlHighlighter::setTextDocument(QQuickTextDocument *document) {
    if (document == m_textDocument)
        return;
    m_textDocument = document;
    setDocument(document ? document->textDocument() : nullptr);
    emit textDocumentChanged();
}

void SqlHighlighter::setTables(const QStringList &tables) {
    if (tables == m_tableList)
        return;
    m_tableList = tables;
    m_tables.clear();
    for (const auto &t : tables)
        m_tables.insert(t.toLower());
    emit changed();
}

void SqlHighlighter::highlightBlock(const QString &text) {
    const int n = text.size();
    int i = 0;
    int state = previousBlockState();
    setCurrentBlockState(Normal);

    // Continue a comment or string that started on an earlier line.
    auto closeQuote = [&](QChar quote, int from) {
        int j = from;
        while (j < n) {
            if (text[j] == quote) {
                if (j + 1 < n && text[j + 1] == quote) {
                    j += 2;
                    continue;
                }
                return j + 1;
            }
            ++j;
        }
        return -1;
    };
    if (state == InComment) {
        const int e = text.indexOf("*/");
        setFormat(0, e < 0 ? n : e + 2, styled(m_comment, false, true));
        if (e < 0) {
            setCurrentBlockState(InComment);
            return;
        }
        i = e + 2;
    } else if (state == InString || state == InQuoted) {
        const QChar quote = state == InString ? '\'' : '"';
        const int e = closeQuote(quote, 0);
        setFormat(0, e < 0 ? n : e, styled(state == InString ? m_string : m_table));
        if (e < 0) {
            setCurrentBlockState(state);
            return;
        }
        i = e;
    }

    while (i < n) {
        const QChar c = text[i];
        if (c == '-' && i + 1 < n && text[i + 1] == '-') {
            setFormat(i, n - i, styled(m_comment, false, true));
            return;
        }
        if (c == '/' && i + 1 < n && text[i + 1] == '*') {
            const int e = text.indexOf("*/", i + 2);
            setFormat(i, (e < 0 ? n : e + 2) - i, styled(m_comment, false, true));
            if (e < 0) {
                setCurrentBlockState(InComment);
                return;
            }
            i = e + 2;
            continue;
        }
        if (c == '\'' || c == '"' || c == '`') {
            const int e = closeQuote(c, i + 1);
            const int end = e < 0 ? n : e;
            if (c == '\'') {
                setFormat(i, end - i, styled(m_string));
            } else {
                const QString name = text.mid(i + 1, end - i - 2).toLower();
                if (m_tables.contains(name))
                    setFormat(i, end - i, styled(m_table));
            }
            if (e < 0) {
                if (c != '`')
                    setCurrentBlockState(c == '\'' ? InString : InQuoted);
                return;
            }
            i = e;
            continue;
        }
        if (c.isDigit() || (c == '.' && i + 1 < n && text[i + 1].isDigit())) {
            const bool word = i > 0 && (text[i - 1].isLetterOrNumber() || text[i - 1] == '_');
            int j = i;
            while (j < n && (text[j].isLetterOrNumber() || text[j] == '.'))
                ++j;
            if (!word)
                setFormat(i, j - i, styled(m_number));
            i = j;
            continue;
        }
        if (c == '?' || ((c == ':' || c == '@' || c == '$') && i + 1 < n && text[i + 1].isLetter())) {
            int j = i + 1;
            while (j < n && (text[j].isLetterOrNumber() || text[j] == '_'))
                ++j;
            setFormat(i, j - i, styled(m_parameter));
            i = j;
            continue;
        }
        if (c.isLetter() || c == '_') {
            int j = i;
            while (j < n && (text[j].isLetterOrNumber() || text[j] == '_' || text[j] == '$'))
                ++j;
            const QString word = text.mid(i, j - i);
            int k = j;
            while (k < n && text[k].isSpace())
                ++k;
            const bool call = k < n && text[k] == '(';
            const bool qualified = i > 0 && text[i - 1] == '.';
            if (!qualified && m_tables.contains(word.toLower()))
                setFormat(i, j - i, styled(m_table));
            else if (call && SqlText::functions().contains(word.toLower()))
                setFormat(i, j - i, styled(m_function));
            else if (!qualified && SqlText::isKeyword(word.toUpper()))
                setFormat(i, j - i, styled(m_keyword, true));
            i = j;
            continue;
        }
        ++i;
    }
}
