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

bool isSameGiftBurst(const LiveChatEntry &left, const LiveChatEntry &right)
{
    return left.kind == QLatin1String("gift") && right.kind == QLatin1String("gift")
           && left.account == right.account && left.giftId == right.giftId
           && left.giftId > 0;
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
        if (entry.kind == QLatin1String("gift"))
            return giftTextForEntry(entry);
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

QString LiveChatModel::giftTextForEntry(const LiveChatEntry &entry) const
{
    const QString giftName = giftNameForId(entry.giftId);
    return entry.giftCount > 1 ? QStringLiteral("%1 x%2").arg(giftName).arg(entry.giftCount)
                               : giftName;
}

QString LiveChatModel::giftNameForId(int giftId) const
{
    const auto it = m_giftNames.constFind(giftId);
    if (it != m_giftNames.constEnd() && !it.value().isEmpty())
        return it.value();
    return QStringLiteral("#%1").arg(giftId);
}

void LiveChatModel::ingestGiftList(const QJsonObject &giftList)
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
    qCInfo(lcShowroomLive) << "Gift list loaded," << m_giftNames.size() << "entries";

    refreshGiftRows();
}

void LiveChatModel::refreshGiftRows()
{
    if (m_entries.isEmpty())
        return;

    int firstGiftRow = -1;
    int lastGiftRow = -1;
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).kind != QLatin1String("gift"))
            continue;
        if (firstGiftRow < 0)
            firstGiftRow = row;
        lastGiftRow = row;
    }

    if (firstGiftRow >= 0) {
        emit dataChanged(index(firstGiftRow), index(lastGiftRow), {TextRole});
        qCDebug(lcShowroomLive) << "Refreshed gift rows" << firstGiftRow << ".." << lastGiftRow;
    }
}

bool LiveChatModel::parsePayload(const QJsonObject &payload, LiveChatEntry *entry) const
{
    if (!entry || payload.isEmpty())
        return false;

    entry->typeCode = parseTypeCode(payload);
    entry->timestamp = parseTimestamp(payload);
    entry->account = readAccount(payload);

    if (payload.contains(QLatin1String("cm"))) {
        entry->kind = QStringLiteral("comment");
        entry->text = payload.value(QLatin1String("cm")).toString();
        return !entry->text.isEmpty();
    }

    if (payload.contains(QLatin1String("g"))) {
        entry->kind = QStringLiteral("gift");
        entry->giftId = readJsonInt(payload.value(QLatin1String("g")));
        entry->giftCount = readJsonInt(payload.value(QLatin1String("n")));
        if (entry->giftCount < 1)
            entry->giftCount = 1;
        return entry->giftId > 0;
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

    if (tryMergeGift(entry)) {
        scheduleFlush();
        return;
    }

    m_pending.append(entry);
    scheduleFlush();
}

bool LiveChatModel::tryMergeGift(const LiveChatEntry &entry)
{
    if (entry.kind != QLatin1String("gift"))
        return false;

    if (!m_pending.isEmpty() && isSameGiftBurst(m_pending.last(), entry)) {
        m_pending.last().giftCount += entry.giftCount;
        return true;
    }

    if (!m_entries.isEmpty() && isSameGiftBurst(m_entries.last(), entry)) {
        LiveChatEntry &last = m_entries.last();
        last.giftCount += entry.giftCount;
        const int row = m_entries.size() - 1;
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx, {TextRole});
        return true;
    }

    return false;
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

void LiveChatModel::clearGiftNames()
{
    if (m_giftNames.isEmpty())
        return;

    m_giftNames.clear();
    refreshGiftRows();
    qCInfo(lcShowroomLive) << "Gift name map cleared";
}
