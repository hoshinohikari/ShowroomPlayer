#include "LiveChatModel.h"
#include "ShowroomLog.h"

#include <QJsonArray>

namespace {

int parseTypeCode(const QJsonObject &payload)
{
    const QJsonValue typeValue = payload.value(QLatin1String("t"));
    if (typeValue.isDouble())
        return typeValue.toInt();
    if (typeValue.isString()) {
        bool ok = false;
        const int parsed = typeValue.toString().toInt(&ok);
        if (ok)
            return parsed;
    }
    return 0;
}

qint64 parseTimestamp(const QJsonObject &payload)
{
    const QJsonValue value = payload.value(QLatin1String("created_at"));
    if (value.isDouble())
        return static_cast<qint64>(value.toDouble());
    if (value.isString()) {
        bool ok = false;
        const qint64 parsed = value.toString().toLongLong(&ok);
        if (ok)
            return parsed;
    }
    return 0;
}

QString readAccount(const QJsonObject &payload)
{
    return payload.value(QLatin1String("ac")).toString().trimmed();
}

} // namespace

LiveChatModel::LiveChatModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_flushTimer(new QTimer(this))
{
    m_flushTimer->setInterval(kFlushIntervalMs);
    m_flushTimer->setSingleShot(true);
    connect(m_flushTimer, &QTimer::timeout, this, &LiveChatModel::flushPending);
}

int LiveChatModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant LiveChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const LiveChatEntry &entry = m_entries.at(index.row());
    switch (role) {
    case KindRole:
        return entry.kind;
    case AccountRole:
        return entry.account;
    case TextRole:
        return entry.text;
    case TypeCodeRole:
        return entry.typeCode;
    case TimestampRole:
        return entry.timestamp;
    default:
        return {};
    }
}

QHash<int, QByteArray> LiveChatModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {AccountRole, "account"},
        {TextRole, "text"},
        {TypeCodeRole, "typeCode"},
        {TimestampRole, "timestamp"},
    };
}

bool LiveChatModel::parsePayload(const QJsonObject &payload, LiveChatEntry *entry) const
{
    if (!entry || payload.isEmpty())
        return false;

    if (payload.contains(QLatin1String("g")))
        return false;

    entry->typeCode = parseTypeCode(payload);
    entry->timestamp = parseTimestamp(payload);
    entry->account = readAccount(payload);

    if (payload.contains(QLatin1String("cm"))) {
        entry->kind = QStringLiteral("comment");
        entry->text = payload.value(QLatin1String("cm")).toString();
        return !entry->text.isEmpty();
    }

    if (payload.contains(QLatin1String("telop")) || payload.contains(QLatin1String("telops"))) {
        entry->kind = QStringLiteral("telop");
        QString telop = payload.value(QLatin1String("telop")).toString().trimmed();
        if (telop.isEmpty()) {
            const QJsonArray telops = payload.value(QLatin1String("telops")).toArray();
            QStringList parts;
            for (const QJsonValue &itemValue : telops) {
                if (!itemValue.isObject())
                    continue;
                const QString part = itemValue.toObject().value(QLatin1String("text")).toString().trimmed();
                if (!part.isEmpty())
                    parts.append(part);
            }
            telop = parts.join(QString());
        }
        entry->text = telop;
        entry->account.clear();
        return !entry->text.isEmpty();
    }

    return false;
}

void LiveChatModel::ingestPayload(const QJsonObject &payload)
{
    LiveChatEntry entry;
    if (!parsePayload(payload, &entry))
        return;

    m_pending.append(entry);
    scheduleFlush();
}

void LiveChatModel::scheduleFlush()
{
    if (!m_flushTimer->isActive())
        m_flushTimer->start();
}

void LiveChatModel::trimExcessBeforeInsert(int incomingCount)
{
    const int total = m_entries.size() + incomingCount;
    if (total <= kMaxEntries)
        return;

    const int removeCount = total - kMaxEntries;
    beginRemoveRows({}, 0, removeCount - 1);
    m_entries.erase(m_entries.begin(), m_entries.begin() + removeCount);
    endRemoveRows();
}

void LiveChatModel::flushPending()
{
    if (m_pending.isEmpty())
        return;

    const int batchSize = qMin(m_pending.size(), kMaxPendingPerFlush);
    QList<LiveChatEntry> batch;
    batch.reserve(batchSize);
    for (int i = 0; i < batchSize; ++i)
        batch.append(m_pending.at(i));
    m_pending.remove(0, batchSize);

    trimExcessBeforeInsert(batch.size());

    const int firstRow = m_entries.size();
    const int lastRow = firstRow + batch.size() - 1;
    beginInsertRows({}, firstRow, lastRow);
    m_entries.append(batch);
    endInsertRows();

    qCDebug(lcShowroomLive) << "Chat flush:" << batch.size() << "rows,"
                            << m_pending.size() << "queued," << m_entries.size() << "visible";

    emit messagesFlushed();

    if (!m_pending.isEmpty())
        m_flushTimer->start();
}

void LiveChatModel::clearMessages()
{
    m_flushTimer->stop();
    m_pending.clear();

    if (m_entries.isEmpty())
        return;

    beginResetModel();
    m_entries.clear();
    endResetModel();
    qCInfo(lcShowroomLive) << "Live chat messages cleared";
}
