#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

struct sqlite3;

struct ColumnInfo {
    QString name;
    QString type;
    bool primaryKey = false;
    bool notNull = false;
    QString defaultValue;
    QString references; // "customers.id" for a foreign key
};

struct TableInfo {
    QString name;
    QString kind; // "table" or "view"
    QList<ColumnInfo> columns;
    QStringList indexes;
    qint64 rows = -1;
    QString sql;
};

struct QueryResult {
    bool ok = true;
    bool interrupted = false;
    QString error;
    int errorOffset = -1; // character offset into the SQL that was run
    bool hasRows = false; // the last statement that returns rows, if any
    QStringList columns;
    QStringList types;
    QList<QVariantList> rows;
    bool truncated = false;
    qint64 changes = 0;
    int statements = 0;
    qint64 elapsedMs = 0;
};

// One SQLite connection. execute() may run on a worker thread while the
// owner calls interrupt(); SQLite's default serialized mode makes that safe.
class Database {
public:
    Database() = default;
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool open(const QString &path, QString *error);
    void close();
    bool isOpen() const { return m_db; }
    QString path() const { return m_path; }
    // True when SQLite opened the file without write access: a URI with
    // mode=ro or immutable=1, or a file the user can't write to.
    bool isReadOnly() const;

    QList<TableInfo> schema() const;
    QueryResult execute(const QString &sql, int maxRows) const;
    // True when every statement in sql only reads, so running it again (to
    // fetch more rows) changes nothing. Not while execute() runs.
    bool readOnly(const QString &sql) const;
    void interrupt() const;

private:
    sqlite3 *m_db = nullptr;
    QString m_path;
};
