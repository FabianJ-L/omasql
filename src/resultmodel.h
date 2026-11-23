#pragma once

#include <QAbstractTableModel>
#include <QStringList>
#include <QVariant>

// The rows of the last query, for the grid and the row inspector. Values stay
// as SQLite returned them (integer, real, text, blob, null) so sorting and
// formatting can tell them apart. A filter narrows the rows shown without
// running the query again; copies and exports take the rows shown.
class ResultModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(int rows READ rowTotal NOTIFY changed)
    Q_PROPERTY(int totalRows READ totalRows NOTIFY changed)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY changed)
    Q_PROPERTY(int columns READ columnTotal NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(int sortColumn READ sortColumn NOTIFY changed)
    Q_PROPERTY(bool sortDescending READ sortDescending NOTIFY changed)

public:
    enum Roles { KindRole = Qt::UserRole + 1 };

    explicit ResultModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

    void setResult(const QStringList &columns, const QStringList &types, const QList<QVariantList> &rows);
    void clear();
    // The same query's rows again, more of them: keeps the columns, the
    // sort, the filter and the revision, so the grid keeps its widths.
    void setRows(const QList<QVariantList> &rows);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int rowTotal() const { return m_rows.size(); }
    int totalRows() const { return m_all.size(); }
    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);
    int columnTotal() const { return m_columns.size(); }
    int revision() const { return m_revision; }
    int sortColumn() const { return m_sortColumn; }
    bool sortDescending() const { return m_descending; }

    Q_INVOKABLE QString columnName(int column) const { return m_columns.value(column); }
    Q_INVOKABLE QString columnType(int column) const { return m_types.value(column); }
    Q_INVOKABLE int columnChars(int column) const;
    Q_INVOKABLE void sortBy(int column);
    Q_INVOKABLE QVariantList rowDetails(int row) const;
    Q_INVOKABLE QString cellText(int row, int column) const;
    Q_INVOKABLE QString rowText(int row) const;
    Q_INVOKABLE QVariant value(int row, int column) const;
    // The block of cells from (top, left) to (bottom, right), tab-separated,
    // the way spreadsheets paste it.
    Q_INVOKABLE QString rangeText(int top, int left, int bottom, int right) const;
    QString toCsv() const;
    QString toJson() const;
    QString toMarkdown() const;
    QString toSqlInsert(const QString &table) const;
    // The rows shown in one of the formats above: "csv", "json", "markdown", "sql".
    QString format(const QString &format, const QString &table = {}) const;

    static QString kindOf(const QVariant &value);
    static QString displayText(const QVariant &value);
    static QString fullText(const QVariant &value, bool *json = nullptr);
    static QString sqlLiteral(const QVariant &value);

signals:
    void changed();

private:
    QStringList m_columns, m_types;
    void applyFilter();

    void sortRows();

    QList<QVariantList> m_all;  // every row the query returned, in sort order
    QList<QVariantList> m_rows; // the ones the filter lets through
    QString m_filter;
    int m_revision = 0;
    int m_sortColumn = -1;
    bool m_descending = false;
};
