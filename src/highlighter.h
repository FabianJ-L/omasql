#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickTextDocument>
#include <QSet>
#include <QSyntaxHighlighter>

// Colors SQL in the editor's QML TextArea: keywords, strings, numbers,
// comments, functions, and the database's own table names. QML hands it the
// TextArea's textDocument and the theme's colors.
class SqlHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument *textDocument READ textDocument WRITE setTextDocument NOTIFY textDocumentChanged)
    Q_PROPERTY(QStringList tables READ tables WRITE setTables NOTIFY changed)
    Q_PROPERTY(QColor keywordColor MEMBER m_keyword NOTIFY changed)
    Q_PROPERTY(QColor stringColor MEMBER m_string NOTIFY changed)
    Q_PROPERTY(QColor numberColor MEMBER m_number NOTIFY changed)
    Q_PROPERTY(QColor commentColor MEMBER m_comment NOTIFY changed)
    Q_PROPERTY(QColor functionColor MEMBER m_function NOTIFY changed)
    Q_PROPERTY(QColor tableColor MEMBER m_table NOTIFY changed)
    Q_PROPERTY(QColor parameterColor MEMBER m_parameter NOTIFY changed)

public:
    explicit SqlHighlighter(QObject *parent = nullptr);

    QQuickTextDocument *textDocument() const { return m_textDocument; }
    void setTextDocument(QQuickTextDocument *document);
    QStringList tables() const { return m_tableList; }
    void setTables(const QStringList &tables);

signals:
    void textDocumentChanged();
    void changed();

protected:
    void highlightBlock(const QString &text) override;

private:
    QPointer<QQuickTextDocument> m_textDocument;
    QStringList m_tableList;
    QSet<QString> m_tables;
    QColor m_keyword, m_string, m_number, m_comment, m_function, m_table, m_parameter;
};
