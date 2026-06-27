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
{
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
    for (const QLatin1StringView key : {QLatin1StringView("enquete"), QLatin1StringView("normal")}) {
        const QJsonArray items = giftList.value(key).toArray();
        for (const QJsonValue &itemValue : items) {
            if (!itemValue.isObject())
                continue;
            const QJsonObject item = itemValue.toObject();
            const int giftId = item.value(QLatin1String("gift_id")).toInt(0);
            const QString giftName = item.value(QLatin1String("gift_name")).toString().trimmed();
            if (giftId > 0 && !giftName.isEmpty())
                names.insert(giftId, giftName);
        }
    }

    m_giftNames = std::move(names);
    qCInfo(lcShowroomLive) << "Gift list loaded," << m_giftNames.size() << "entries";

    if (!m_entries.isEmpty()) {
        for (int row = 0; row < m_entries.size(); ++row) {
            if (m_entries.at(row).kind == QLatin1String("gift")) {
                const QModelIndex idx = index(row);
                emit dataChanged(idx, idx, {TextRole});
            }
        }
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
        entry->giftId = payload.value(QLatin1String("g")).toInt(0);
        entry->giftCount = payload.value(QLatin1String("n")).toInt(1);
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

    appendEntry(entry);
}

void LiveChatModel::appendEntry(const LiveChatEntry &entry)
{
    if (m_entries.size() >= kMaxEntries) {
        beginRemoveRows({}, 0, 0);
        m_entries.removeFirst();
        endRemoveRows();
    }

    const int row = m_entries.size();
    beginInsertRows({}, row, row);
    m_entries.append(entry);
    endInsertRows();

    qCDebug(lcShowroomLive) << "Chat" << entry.kind << entry.account
                            << (entry.kind == QLatin1String("gift") ? giftTextForEntry(entry)
                                                                    : entry.text.left(80));
    emit messageAppended();
}

void LiveChatModel::clear()
{
    if (m_entries.isEmpty())
        return;

    beginResetModel();
    m_entries.clear();
    m_giftNames.clear();
    endResetModel();
    qCInfo(lcShowroomLive) << "Live chat cleared";
}
