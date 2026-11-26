#include "database.h"

#include "sqltext.h"

#include <QElapsedTimer>
#include <sqlite3.h>

namespace {

QVariant columnValue(sqlite3_stmt *stmt, int column) {
    switch (sqlite3_column_type(stmt, column)) {
    case SQLITE_INTEGER:
        return QVariant::fromValue<qint64>(sqlite3_column_int64(stmt, column));
    case SQLITE_FLOAT:
        return sqlite3_column_double(stmt, column);
    case SQLITE_TEXT:
        return QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, column)),
                                 sqlite3_column_bytes(stmt, column));
    case SQLITE_BLOB:
        return QByteArray(static_cast<const char *>(sqlite3_column_blob(stmt, column)),
                          sqlite3_column_bytes(stmt, column));
    default:
        return {};
    }
}

// A small read for schema lookups, with one text parameter.
QList<QVariantList> select(sqlite3 *db, const QString &sql, const QString &parameter = {}) {
    QList<QVariantList> rows;
    sqlite3_stmt *stmt = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db, utf8.constData(), utf8.size(), &stmt, nullptr) != SQLITE_OK)
        return rows;
    const QByteArray bound = parameter.toUtf8();
    if (sqlite3_bind_parameter_count(stmt) > 0)
        sqlite3_bind_text(stmt, 1, bound.constData(), bound.size(), SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantList row;
        for (int i = 0; i < sqlite3_column_count(stmt); ++i)
            row << columnValue(stmt, i);
        rows << row;
    }
    sqlite3_finalize(stmt);
    return rows;
}

} // namespace

Database::~Database() { close(); }

bool Database::open(const QString &path, QString *error) {
    close();
    sqlite3 *db = nullptr;
    const int rc = sqlite3_open_v2(path.toUtf8().constData(), &db,
                                   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI, nullptr);
    if (rc != SQLITE_OK) {
        if (error)
            *error = db ? QString::fromUtf8(sqlite3_errmsg(db)) : QString::fromUtf8(sqlite3_errstr(rc));
        sqlite3_close(db);
        return false;
    }
    // SQLite opens any file lazily; reading the schema is what tells a
    // database from a random file.
    char *message = nullptr;
    if (sqlite3_exec(db, "SELECT count(*) FROM sqlite_schema", nullptr, nullptr, &message) != SQLITE_OK) {
        if (error)
            *error = QString::fromUtf8(message ? message : sqlite3_errmsg(db));
        sqlite3_free(message);
        sqlite3_close(db);
        return false;
    }
    sqlite3_busy_timeout(db, 3000);
    m_db = db;
    m_path = path;
    return true;
}

bool Database::isReadOnly() const {
    return m_db && sqlite3_db_readonly(m_db, "main") == 1;
}

void Database::close() {
    if (m_db)
        sqlite3_close_v2(m_db);
    m_db = nullptr;
    m_path.clear();
}

QList<TableInfo> Database::schema() const {
    QList<TableInfo> tables;
    if (!m_db)
        return tables;
    const auto entries = select(m_db,
        "SELECT name, type, sql FROM sqlite_schema WHERE type IN ('table', 'view') "
        "AND name NOT LIKE 'sqlite_%' ORDER BY type = 'view', name COLLATE NOCASE");
    for (const auto &entry : entries) {
        TableInfo table;
        table.name = entry.value(0).toString();
        table.kind = entry.value(1).toString();
        table.sql = entry.value(2).toString();

        QHash<QString, QString> references;
        for (const auto &fk : select(m_db, "SELECT \"from\", \"table\", \"to\" FROM pragma_foreign_key_list(?1)", table.name))
            references.insert(fk.value(0).toString(),
                              fk.value(1).toString() + (fk.value(2).isNull() ? QString() : "." + fk.value(2).toString()));

        for (const auto &c : select(m_db, "SELECT name, type, \"notnull\", dflt_value, pk FROM pragma_table_info(?1)", table.name)) {
            ColumnInfo column;
            column.name = c.value(0).toString();
            column.type = c.value(1).toString();
            column.notNull = c.value(2).toInt();
            column.defaultValue = c.value(3).toString();
            column.primaryKey = c.value(4).toInt() > 0;
            column.references = references.value(column.name);
            table.columns << column;
        }
        for (const auto &index : select(m_db, "SELECT name FROM pragma_index_list(?1)", table.name))
            table.indexes << index.value(0).toString();
        if (table.kind == "table") {
            const auto count = select(m_db, "SELECT count(*) FROM " + SqlText::quoteIdentifier(table.name));
            if (!count.isEmpty())
                table.rows = count.first().value(0).toLongLong();
        }
        tables << table;
    }
    return tables;
}

QueryResult Database::execute(const QString &sql, int maxRows) const {
    QueryResult result;
    QElapsedTimer timer;
    timer.start();
    if (!m_db) {
        result.ok = false;
        result.error = "No database is open.";
        return result;
    }

    const QByteArray utf8 = sql.toUtf8();
    const char *head = utf8.constData();
    const char *end = head + utf8.size();
    const char *tail = head;
    auto characterOffset = [&](qsizetype bytes) { return int(QString::fromUtf8(head, bytes).size()); };
    const sqlite3_int64 changesBefore = sqlite3_total_changes64(m_db);

    while (tail < end) {
        sqlite3_stmt *stmt = nullptr;
        const char *next = nullptr;
        const char *statementStart = tail;
        if (sqlite3_prepare_v2(m_db, tail, int(end - tail), &stmt, &next) != SQLITE_OK) {
            result.ok = false;
            result.error = QString::fromUtf8(sqlite3_errmsg(m_db));
            const int offset = sqlite3_error_offset(m_db);
            result.errorOffset = characterOffset((statementStart - head) + qMax(0, offset));
            break;
        }
        tail = next;
        if (!stmt)
            continue; // only whitespace or a comment
        ++result.statements;

        const int columns = sqlite3_column_count(stmt);
        if (columns > 0) {
            result.hasRows = true;
            result.columns.clear();
            result.types.clear();
            result.rows.clear();
            result.truncated = false;
            for (int i = 0; i < columns; ++i) {
                result.columns << QString::fromUtf8(sqlite3_column_name(stmt, i));
                const char *type = sqlite3_column_decltype(stmt, i);
                result.types << (type ? QString::fromUtf8(type) : QString());
            }
        }

        int rc;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            if (result.rows.size() >= maxRows) {
                result.truncated = true;
                rc = SQLITE_DONE;
                break;
            }
            QVariantList row;
            row.reserve(columns);
            for (int i = 0; i < columns; ++i)
                row << columnValue(stmt, i);
            result.rows << row;
        }
        if (rc != SQLITE_DONE) {
            result.ok = false;
            result.interrupted = rc == SQLITE_INTERRUPT;
            result.error = QString::fromUtf8(sqlite3_errmsg(m_db));
            result.errorOffset = characterOffset(statementStart - head);
            sqlite3_finalize(stmt);
            break;
        }
        sqlite3_finalize(stmt);
    }

    result.changes = sqlite3_total_changes64(m_db) - changesBefore;
    result.elapsedMs = timer.elapsed();
    return result;
}

bool Database::readOnly(const QString &sql) const {
    if (!m_db)
        return false;
    const QByteArray utf8 = sql.toUtf8();
    const char *tail = utf8.constData();
    const char *end = tail + utf8.size();
    bool any = false;
    while (tail < end) {
        sqlite3_stmt *stmt = nullptr;
        const char *next = nullptr;
        if (sqlite3_prepare_v2(m_db, tail, int(end - tail), &stmt, &next) != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return false;
        }
        tail = next;
        if (!stmt)
            continue;
        const bool reads = sqlite3_stmt_readonly(stmt);
        sqlite3_finalize(stmt);
        if (!reads)
            return false;
        any = true;
    }
    return any;
}

void Database::interrupt() const {
    if (m_db)
        sqlite3_interrupt(m_db);
}
