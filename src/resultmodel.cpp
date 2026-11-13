#include "resultmodel.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QLocale>
#include <algorithm>

namespace {

QString size(qint64 bytes) {
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

int rank(const QVariant &value) {
    switch (value.typeId()) {
    case QMetaType::UnknownType: return 0;
    case QMetaType::LongLong:
    case QMetaType::Double: return 1;
    case QMetaType::QString: return 2;
    default: return 3;
    }
}

// SQLite's own ordering: NULL, then numbers, then text, then blobs.
int compare(const QVariant &a, const QVariant &b) {
    const int ra = rank(a), rb = rank(b);
    if (ra != rb)
        return ra < rb ? -1 : 1;
    switch (ra) {
    case 1: {
        if (a.typeId() == QMetaType::LongLong && b.typeId() == QMetaType::LongLong) {
            const qint64 x = a.toLongLong(), y = b.toLongLong();
            return x < y ? -1 : x > y;
        }
        const double x = a.toDouble(), y = b.toDouble();
        return x < y ? -1 : x > y;
    }
    case 2: return QString::localeAwareCompare(a.toString(), b.toString());
    case 3: return a.toByteArray().compare(b.toByteArray());
    default: return 0;
    }
}

QString csvField(const QVariant &value) {
    if (value.isNull())
        return {};
    QString text = value.typeId() == QMetaType::QByteArray ? QString::fromLatin1(value.toByteArray().toHex())
                                                           : ResultModel::fullText(value);
    if (value.typeId() == QMetaType::QString)
        text = value.toString();
    if (text.contains(',') || text.contains('"') || text.contains('\n') || text.contains('\r'))
        text = '"' + text.replace('"', "\"\"") + '"';
    return text;
}

} // namespace

void ResultModel::setResult(const QStringList &columns, const QStringList &types, const QList<QVariantList> &rows) {
    beginResetModel();
    m_columns = columns;
    m_types = types;
    m_rows = rows;
    m_sortColumn = -1;
    m_descending = false;
    ++m_revision;
    endResetModel();
    emit changed();
}

void ResultModel::clear() { setResult({}, {}, {}); }

int ResultModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_rows.size(); }
int ResultModel::columnCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_columns.size(); }

QVariant ResultModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const QVariant value = m_rows[index.row()].value(index.column());
    if (role == Qt::DisplayRole)
        return displayText(value);
    if (role == KindRole)
        return kindOf(value);
    return {};
}

QVariant ResultModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return {};
    if (orientation == Qt::Horizontal)
        return m_columns.value(section);
    return section + 1;
}

QHash<int, QByteArray> ResultModel::roleNames() const {
    return {{Qt::DisplayRole, "display"}, {KindRole, "kind"}};
}

QString ResultModel::kindOf(const QVariant &value) {
    switch (value.typeId()) {
    case QMetaType::UnknownType: return "null";
    case QMetaType::LongLong: return "integer";
    case QMetaType::Double: return "real";
    case QMetaType::QByteArray: return "blob";
    default: return "text";
    }
}

QString ResultModel::displayText(const QVariant &value) {
    switch (value.typeId()) {
    case QMetaType::UnknownType: return "NULL";
    case QMetaType::Double: return QString::number(value.toDouble(), 'g', QLocale::FloatingPointShortest);
    case QMetaType::QByteArray:
        return QCoreApplication::translate("ResultModel", "BLOB · %1").arg(size(value.toByteArray().size()));
    case QMetaType::QString: {
        QString text = value.toString();
        if (text.size() > 300)
            text = text.left(300) + QStringLiteral("…");
        return text.replace("\r\n", "⏎ ").replace('\n', "⏎ ").replace('\t', ' ');
    }
    default: return value.toString();
    }
}

// The whole value for the inspector: JSON indented, blobs as a hex dump.
QString ResultModel::fullText(const QVariant &value, bool *json) {
    if (json)
        *json = false;
    switch (value.typeId()) {
    case QMetaType::UnknownType: return "NULL";
    case QMetaType::Double: return QString::number(value.toDouble(), 'g', QLocale::FloatingPointShortest);
    case QMetaType::QByteArray: {
        const QByteArray bytes = value.toByteArray();
        QStringList lines;
        const int shown = qMin<qsizetype>(bytes.size(), 1024);
        for (int i = 0; i < shown; i += 16) {
            const QByteArray chunk = bytes.mid(i, 16);
            QString ascii;
            for (char c : chunk)
                ascii += (c >= 32 && c < 127) ? QChar(c) : QChar('.');
            lines << QString("%1  %2  %3").arg(i, 6, 16, QChar('0'))
                         .arg(QString::fromLatin1(chunk.toHex(' ')), -47).arg(ascii);
        }
        if (bytes.size() > shown)
            lines << QCoreApplication::translate("ResultModel", "… %1 more").arg(size(bytes.size() - shown));
        return lines.join('\n');
    }
    case QMetaType::QString: {
        const QString text = value.toString();
        const QString trimmed = text.trimmed();
        if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(trimmed.toUtf8(), &error);
            if (error.error == QJsonParseError::NoError) {
                if (json)
                    *json = true;
                return QString::fromUtf8(document.toJson(QJsonDocument::Indented)).trimmed();
            }
        }
        return text;
    }
    default: return value.toString();
    }
}

int ResultModel::columnChars(int column) const {
    int chars = m_columns.value(column).size();
    chars = qMax(chars, m_types.value(column).size());
    const int sample = qMin<qsizetype>(m_rows.size(), 300);
    for (int r = 0; r < sample; ++r)
        chars = qMax(chars, int(displayText(m_rows[r].value(column)).size()));
    return qMin(chars, 60);
}

void ResultModel::sortBy(int column) {
    if (column < 0 || column >= m_columns.size())
        return;
    // Ascending, descending, then back to the order the query returned isn't
    // possible once sorted, so it simply toggles.
    const bool descending = m_sortColumn == column ? !m_descending : false;
    beginResetModel();
    std::stable_sort(m_rows.begin(), m_rows.end(), [&](const QVariantList &a, const QVariantList &b) {
        const int c = compare(a.value(column), b.value(column));
        return descending ? c > 0 : c < 0;
    });
    m_sortColumn = column;
    m_descending = descending;
    endResetModel();
    emit changed();
}

QVariantList ResultModel::rowDetails(int row) const {
    QVariantList details;
    if (row < 0 || row >= m_rows.size())
        return details;
    for (int c = 0; c < m_columns.size(); ++c) {
        const QVariant value = m_rows[row].value(c);
        bool json = false;
        const QString text = fullText(value, &json);
        details << QVariantMap{{"name", m_columns[c]}, {"type", m_types.value(c)}, {"kind", kindOf(value)},
                               {"value", text}, {"json", json}};
    }
    return details;
}

QString ResultModel::cellText(int row, int column) const {
    if (row < 0 || row >= m_rows.size())
        return {};
    const QVariant value = m_rows[row].value(column);
    if (value.isNull())
        return {};
    if (value.typeId() == QMetaType::QByteArray)
        return QString::fromLatin1(value.toByteArray().toHex());
    if (value.typeId() == QMetaType::QString)
        return value.toString();
    return fullText(value);
}

QString ResultModel::rowText(int row) const {
    QStringList fields;
    for (int c = 0; c < m_columns.size(); ++c)
        fields << cellText(row, c);
    return fields.join('\t');
}

QString ResultModel::toCsv() const {
    QStringList lines;
    QStringList header;
    for (const auto &c : m_columns)
        header << csvField(c);
    lines << header.join(',');
    for (const auto &row : m_rows) {
        QStringList fields;
        for (int c = 0; c < m_columns.size(); ++c)
            fields << csvField(row.value(c));
        lines << fields.join(',');
    }
    return lines.join("\r\n") + "\r\n";
}
