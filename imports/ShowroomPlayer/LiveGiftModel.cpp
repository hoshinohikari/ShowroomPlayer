#include "LiveGiftModel.h"
#include "ShowroomLog.h"

#include <QJsonArray>

namespace {

int readJsonInt(const QJsonValue &value)
{
    if (value.isDouble())
        return value.toInt();
    if (value.isString()) {
        bool ok = false;
        const int parsed = value.toString().toInt(&ok);
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

bool isSameGiftBurst(const LiveGiftEntry &left, const LiveGiftEntry &right)
{
    return left.account == right.account && left.giftId == right.giftId && left.giftId > 0;
}

} // namespace

LiveGiftModel::LiveGiftModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_flushTimer(new QTimer(this))
{
    m_flushTimer->setInterval(kFlushIntervalMs);
    m_flushTimer->setSingleShot(true);
    connect(m_flushTimer, &QTimer::timeout, this, &LiveGiftModel::flushPending);
}

int LiveGiftModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant LiveGiftModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const LiveGiftEntry &entry = m_entries.at(index.row());
    switch (role) {
    case AccountRole:
        return entry.account;
    case TextRole:
        return giftTextForEntry(entry);
    case GiftIdRole:
        return entry.giftId;
    case GiftCountRole:
        return entry.giftCount;
    case TimestampRole:
        return entry.timestamp;
    default:
        return {};
    }
}

QHash<int, QByteArray> LiveGiftModel::roleNames() const
{
    return {
        {AccountRole, "account"},
        {TextRole, "text"},
        {GiftIdRole, "giftId"},
        {GiftCountRole, "giftCount"},
        {TimestampRole, "timestamp"},
    };
}

QString LiveGiftModel::giftTextForEntry(const LiveGiftEntry &entry) const
{
    const QString giftName = giftNameForId(entry.giftId);
    return entry.giftCount > 1 ? QStringLiteral("%1 x%2").arg(giftName).arg(entry.giftCount)
                               : giftName;
}

QString LiveGiftModel::giftNameForId(int giftId) const
{
    const auto it = m_giftNames.constFind(giftId);
    if (it != m_giftNames.constEnd() && !it.value().isEmpty())
        return it.value();
    return QStringLiteral("#%1").arg(giftId);
}

void LiveGiftModel::ingestGiftList(const QJsonObject &giftList)
{
    QHash<int, QString> names;
    for (auto it = giftList.begin(); it != giftList.end(); ++it) {
        if (!it.value().isArray())
            continue;

        const QJsonArray items = it.value().toArray();
        for (const QJsonValue &itemValue : items) {
            if (!itemValue.isObject())
                continue;
            const QJsonObject item = itemValue.toObject();
            const int giftId = readJsonInt(item.value(QLatin1String("gift_id")));
            const QString giftName = item.value(QLatin1String("gift_name")).toString().trimmed();
            if (giftId > 0 && !giftName.isEmpty())
                names.insert(giftId, giftName);
        }
    }

    m_giftNames = std::move(names);
    qCInfo(lcShowroomLive) << "Gift panel names loaded," << m_giftNames.size() << "entries";
    refreshGiftRows();
}

void LiveGiftModel::refreshGiftRows()
{
    if (m_entries.isEmpty())
        return;

    emit dataChanged(index(0), index(m_entries.size() - 1), {TextRole});
}

bool LiveGiftModel::parseGiftPayload(const QJsonObject &payload, LiveGiftEntry *entry) const
{
    if (!entry || !payload.contains(QLatin1String("g")))
        return false;

    entry->account = payload.value(QLatin1String("ac")).toString().trimmed();
    entry->giftId = readJsonInt(payload.value(QLatin1String("g")));
    entry->giftCount = readJsonInt(payload.value(QLatin1String("n")));
    entry->timestamp = parseTimestamp(payload);
    if (entry->giftCount < 1)
        entry->giftCount = 1;
    return entry->giftId > 0;
}

void LiveGiftModel::ingestPayload(const QJsonObject &payload)
{
    LiveGiftEntry entry;
    if (!parseGiftPayload(payload, &entry))
        return;

    if (tryMergeGift(entry)) {
        scheduleFlush();
        return;
    }

    m_pending.append(entry);
    scheduleFlush();
}

bool LiveGiftModel::tryMergeGift(const LiveGiftEntry &entry)
{
    if (!m_pending.isEmpty() && isSameGiftBurst(m_pending.last(), entry)) {
        m_pending.last().giftCount += entry.giftCount;
        return true;
    }

    if (!m_entries.isEmpty() && isSameGiftBurst(m_entries.last(), entry)) {
        LiveGiftEntry &last = m_entries.last();
        last.giftCount += entry.giftCount;
        const int row = m_entries.size() - 1;
        emit dataChanged(index(row), index(row), {TextRole, GiftCountRole});
        return true;
    }

    return false;
}

void LiveGiftModel::scheduleFlush()
{
    if (!m_flushTimer->isActive())
        m_flushTimer->start();
}

void LiveGiftModel::trimExcessBeforeInsert(int incomingCount)
{
    const int total = m_entries.size() + incomingCount;
    if (total <= kMaxEntries)
        return;

    const int removeCount = total - kMaxEntries;
    beginRemoveRows({}, 0, removeCount - 1);
    m_entries.erase(m_entries.begin(), m_entries.begin() + removeCount);
    endRemoveRows();
}

void LiveGiftModel::flushPending()
{
    if (m_pending.isEmpty())
        return;

    const int batchSize = qMin(m_pending.size(), kMaxPendingPerFlush);
    QList<LiveGiftEntry> batch;
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

    qCDebug(lcShowroomLive) << "Gift flush:" << batch.size() << "rows,"
                            << m_pending.size() << "queued," << m_entries.size() << "visible";

    emit messagesFlushed();

    if (!m_pending.isEmpty())
        m_flushTimer->start();
}

void LiveGiftModel::clearMessages()
{
    m_flushTimer->stop();
    m_pending.clear();

    if (m_entries.isEmpty())
        return;

    beginResetModel();
    m_entries.clear();
    endResetModel();
    qCInfo(lcShowroomLive) << "Gift panel messages cleared";
}

void LiveGiftModel::clearGiftNames()
{
    if (m_giftNames.isEmpty())
        return;

    m_giftNames.clear();
    refreshGiftRows();
    qCInfo(lcShowroomLive) << "Gift panel name map cleared";
}
