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

qint64 readJsonInt64(const QJsonValue &value)
{
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

qint64 readUserId(const QJsonObject &payload)
{
    for (const QLatin1StringView key :
         {QLatin1StringView("ui"), QLatin1StringView("user_id"), QLatin1StringView("uid")}) {
        const qint64 userId = readJsonInt64(payload.value(key));
        if (userId > 0)
            return userId;
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
    connect(&m_contributors, &GiftContributorModel::rankingChanged, this,
            &LiveGiftModel::onRankingChanged);
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
    case PtRole:
        return entry.ptEarned;
    case RankRole:
        return m_contributors.rankForAccount(entry.account);
    case TotalPtRole:
        return m_contributors.totalPtForAccount(entry.account);
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
        {PtRole, "pt"},
        {RankRole, "rank"},
        {TotalPtRole, "totalPt"},
        {TimestampRole, "timestamp"},
    };
}

QString LiveGiftModel::giftTextForEntry(const LiveGiftEntry &entry) const
{
    const QString giftName = giftNameForId(entry.giftId);
    const QString giftLabel = entry.giftCount > 1
                                  ? QStringLiteral("%1 x%2").arg(giftName).arg(entry.giftCount)
                                  : giftName;
    if (entry.ptEarned > 0)
        return QStringLiteral("%1  +%2pt").arg(giftLabel).arg(entry.ptEarned);
    return giftLabel;
}

QString LiveGiftModel::giftNameForId(int giftId) const
{
    const auto it = m_giftCatalog.constFind(giftId);
    if (it != m_giftCatalog.constEnd() && !it.value().name.isEmpty())
        return it.value().name;
    return QStringLiteral("#%1").arg(giftId);
}

GiftCatalogEntry LiveGiftModel::catalogForGift(int giftId) const
{
    const auto it = m_giftCatalog.constFind(giftId);
    if (it != m_giftCatalog.constEnd())
        return it.value();

    GiftCatalogEntry fallback;
    fallback.name = QStringLiteral("#%1").arg(giftId);
    fallback.point = 1;
    fallback.free = true;
    return fallback;
}

void LiveGiftModel::setEventActive(bool active)
{
    if (m_eventActive == active)
        return;
    m_eventActive = active;
    qCInfo(lcShowroomLive) << "Gift event bonus" << (active ? "enabled" : "disabled");
    emit eventActiveChanged();
}

void LiveGiftModel::ingestGiftList(const QJsonObject &giftList)
{
    QHash<int, GiftCatalogEntry> catalog;
    for (auto it = giftList.begin(); it != giftList.end(); ++it) {
        if (!it.value().isArray())
            continue;

        const QJsonArray items = it.value().toArray();
        for (const QJsonValue &itemValue : items) {
            if (!itemValue.isObject())
                continue;
            const QJsonObject item = itemValue.toObject();
            const int giftId = readJsonInt(item.value(QLatin1String("gift_id")));
            if (giftId <= 0)
                continue;

            GiftCatalogEntry entry;
            entry.name = item.value(QLatin1String("gift_name")).toString().trimmed();
            entry.point = readJsonInt(item.value(QLatin1String("point")));
            entry.free = item.value(QLatin1String("free")).toBool(true);
            entry.giftType = readJsonInt(item.value(QLatin1String("gift_type")));
            if (entry.point <= 0)
                entry.point = 1;
            catalog.insert(giftId, entry);
        }
    }

    m_giftCatalog = std::move(catalog);
    qCInfo(lcShowroomLive) << "Gift catalog loaded," << m_giftCatalog.size() << "entries";
    refreshGiftRows();
}

void LiveGiftModel::refreshGiftRows()
{
    if (m_entries.isEmpty())
        return;

    emit dataChanged(index(0), index(m_entries.size() - 1),
                     {TextRole, PtRole, RankRole, TotalPtRole});
}

void LiveGiftModel::onRankingChanged()
{
    refreshGiftRows();
}

bool LiveGiftModel::parseGiftPayload(const QJsonObject &payload, LiveGiftEntry *entry) const
{
    if (!entry || !payload.contains(QLatin1String("g")))
        return false;

    entry->account = payload.value(QLatin1String("ac")).toString().trimmed();
    entry->userId = readUserId(payload);
    entry->giftId = readJsonInt(payload.value(QLatin1String("g")));
    entry->giftCount = readJsonInt(payload.value(QLatin1String("n")));
    entry->timestamp = parseTimestamp(payload);
    entry->ptEarned = 0;
    if (entry->giftCount < 1)
        entry->giftCount = 1;
    return entry->giftId > 0;
}

QStringList LiveGiftModel::makeGiftDedupKeys(const QString &account, qint64 userId, int giftId,
                                             qint64 timestamp, int count) const
{
    QStringList keys;
    if (userId > 0) {
        keys.append(QStringLiteral("uid:%1|g:%2|t:%3|n:%4")
                        .arg(userId)
                        .arg(giftId)
                        .arg(timestamp)
                        .arg(count));
    }

    const QString normalizedAccount = account.trimmed().toLower();
    if (!normalizedAccount.isEmpty()) {
        keys.append(QStringLiteral("ac:%1|g:%2|t:%3|n:%4")
                        .arg(normalizedAccount)
                        .arg(giftId)
                        .arg(timestamp)
                        .arg(count));
    }

    return keys;
}

bool LiveGiftModel::isDuplicateGift(const QStringList &dedupKeys) const
{
    for (const QString &key : dedupKeys) {
        if (m_processedGiftKeys.contains(key))
            return true;
    }
    return false;
}

void LiveGiftModel::markGiftProcessed(const QStringList &dedupKeys)
{
    for (const QString &key : dedupKeys)
        m_processedGiftKeys.insert(key);
}

int LiveGiftModel::applyContribution(const QString &account, int giftId, int count,
                                     qint64 timestamp, qint64 userId)
{
    if (giftId <= 0 || count < 1)
        return 0;
    if (account.isEmpty() && userId <= 0)
        return 0;

    const QStringList dedupKeys = makeGiftDedupKeys(account, userId, giftId, timestamp, count);
    if (isDuplicateGift(dedupKeys)) {
        qCDebug(lcShowroomLive) << "Skip duplicate gift contribution"
                                << "gift:" << giftId << "count:" << count << "user:" << userId
                                << account;
        return 0;
    }

    const GiftCatalogEntry catalog = catalogForGift(giftId);
    const int pt = ShowroomGiftPt::calculateGiftPt(catalog, count, m_eventActive);
    markGiftProcessed(dedupKeys);
    if (pt > 0 && !account.isEmpty())
        m_contributors.addPoints(account, pt);
    else if (pt > 0 && userId > 0)
        m_contributors.addPoints(QStringLiteral("#%1").arg(userId), pt);
    return pt;
}

void LiveGiftModel::ingestGiftLog(const QJsonObject &giftLogRoot)
{
    const QJsonArray logs = giftLogRoot.value(QLatin1String("gift_log")).toArray();
    if (logs.isEmpty()) {
        qCInfo(lcShowroomLive) << "Gift log is empty";
        return;
    }

    int imported = 0;
    int totalPt = 0;
    for (const QJsonValue &logValue : logs) {
        if (!logValue.isObject())
            continue;

        const QJsonObject item = logValue.toObject();
        const QString account = item.value(QLatin1String("name")).toString().trimmed();
        const qint64 userId = readJsonInt64(item.value(QLatin1String("user_id")));
        const int giftId = readJsonInt(item.value(QLatin1String("gift_id")));
        int count = readJsonInt(item.value(QLatin1String("num")));
        if (count < 1)
            count = 1;

        qint64 timestamp = readJsonInt64(item.value(QLatin1String("created_at")));

        const int pt = applyContribution(account, giftId, count, timestamp, userId);
        if (pt <= 0)
            continue;

        imported++;
        totalPt += pt;
    }

    qCInfo(lcShowroomLive) << "Gift log imported:" << imported << "entries," << totalPt << "pt";
    refreshGiftRows();
}

int LiveGiftModel::recordGiftContribution(LiveGiftEntry *entry)
{
    if (!entry)
        return 0;

    const int pt = applyContribution(entry->account, entry->giftId, entry->giftCount,
                                     entry->timestamp, entry->userId);
    entry->ptEarned = pt;
    return pt;
}

void LiveGiftModel::ingestPayload(const QJsonObject &payload)
{
    LiveGiftEntry entry;
    if (!parseGiftPayload(payload, &entry))
        return;

    recordGiftContribution(&entry);

    if (tryMergeGift(&entry)) {
        scheduleFlush();
        return;
    }

    m_pending.append(entry);
    scheduleFlush();
}

bool LiveGiftModel::tryMergeGift(LiveGiftEntry *entry)
{
    if (!entry)
        return false;

    if (!m_pending.isEmpty() && isSameGiftBurst(m_pending.last(), *entry)) {
        m_pending.last().giftCount += entry->giftCount;
        m_pending.last().ptEarned += entry->ptEarned;
        return true;
    }

    if (!m_entries.isEmpty() && isSameGiftBurst(m_entries.last(), *entry)) {
        LiveGiftEntry &last = m_entries.last();
        last.giftCount += entry->giftCount;
        last.ptEarned += entry->ptEarned;
        const int row = m_entries.size() - 1;
        emit dataChanged(index(row), index(row),
                         {TextRole, GiftCountRole, PtRole, RankRole, TotalPtRole});
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
    m_processedGiftKeys.clear();
    m_contributors.clear();

    if (m_entries.isEmpty())
        return;

    beginResetModel();
    m_entries.clear();
    endResetModel();
    qCInfo(lcShowroomLive) << "Gift panel messages cleared";
}

void LiveGiftModel::clearGiftCatalog()
{
    if (m_giftCatalog.isEmpty() && !m_eventActive)
        return;

    m_giftCatalog.clear();
    m_eventActive = false;
    emit eventActiveChanged();
    refreshGiftRows();
    qCInfo(lcShowroomLive) << "Gift catalog cleared";
}
