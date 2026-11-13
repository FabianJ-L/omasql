#pragma once

#include <QAbstractTableModel>
#include <QStringList>
#include <QVariant>

// The rows of the last query, for the grid and the row inspector. Values stay
// as SQLite returned them (integer, real, text, blob, null) so sorting and
// formatting can tell them apart.
class ResultModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(int rows READ rowTotal NOTIFY changed)
    Q_PROPERTY(int columns READ columnTotal NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(int sortColumn READ sortColumn NOTIFY changed)
    Q_PROPERTY(bool sortDescending READ sortDescending NOTIFY changed)

public:
    enum Roles { KindRole = Qt::UserRole + 1 };

    explicit ResultModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

    void setResult(const QStringList &columns, const QStringList &types, const QList<QVariantList> &rows);
    void clear();

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int rowTotal() const { return m_rows.size(); }
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
    QString toCsv() const;

    static QString kindOf(const QVariant &value);
    static QString displayText(const QVariant &value);
    static QString fullText(const QVariant &value, bool *json = nullptr);

signals:
    void changed();

private:
    QStringList m_columns, m_types;
    QList<QVariantList> m_rows;
    int m_revision = 0;
    int m_sortColumn = -1;
    bool m_descending = false;
};
