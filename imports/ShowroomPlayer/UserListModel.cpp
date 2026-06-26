#include "UserListModel.h"
#include "ShowroomLog.h"

UserListModel::UserListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int UserListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_users.size();
}

QVariant UserListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_users.size())
        return {};

    const MonitoredUser &user = m_users.at(index.row());
    switch (role) {
    case UsernameRole:
        return user.username;
    case RoomIdRole:
        return user.roomId;
    case IsLiveRole:
        return user.isLive;
    default:
        return {};
    }
}

QHash<int, QByteArray> UserListModel::roleNames() const
{
    return {
        {UsernameRole, "username"},
        {RoomIdRole, "roomId"},
        {IsLiveRole, "isLive"},
    };
}

bool UserListModel::containsUsername(const QString &username) const
{
    return indexOfUsername(username) >= 0;
}

int UserListModel::indexOfUsername(const QString &username) const
{
    const QString key = username.trimmed().toLower();
    for (int i = 0; i < m_users.size(); ++i) {
        if (m_users.at(i).username.toLower() == key)
            return i;
    }
    return -1;
}

int UserListModel::appendUser(const QString &username, qint64 roomId)
{
    const int row = m_users.size();
    beginInsertRows({}, row, row);
    m_users.append({username.trimmed(), roomId, false});
    endInsertRows();
    qCDebug(lcShowroomController) << "UserListModel append:" << username.trimmed()
                                  << "roomId:" << roomId << "row:" << row;
    return row;
}

void UserListModel::removeAt(int row)
{
    if (row < 0 || row >= m_users.size())
        return;

    const QString username = m_users.at(row).username;
    beginRemoveRows({}, row, row);
    m_users.removeAt(row);
    qCDebug(lcShowroomController) << "UserListModel remove:" << username << "row:" << row;
    endRemoveRows();
}

void UserListModel::setLive(int row, bool live)
{
    if (row < 0 || row >= m_users.size() || m_users[row].isLive == live)
        return;

    m_users[row].isLive = live;
    qCDebug(lcShowroomController) << "UserListModel setLive:" << m_users[row].username
                                  << "live:" << live;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {IsLiveRole});
}

void UserListModel::setRoomId(int row, qint64 roomId)
{
    if (row < 0 || row >= m_users.size() || m_users[row].roomId == roomId)
        return;

    m_users[row].roomId = roomId;
    qCDebug(lcShowroomController) << "UserListModel setRoomId:" << m_users[row].username
                                  << "roomId:" << roomId;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {RoomIdRole});
}

MonitoredUser UserListModel::userAt(int row) const
{
    if (row < 0 || row >= m_users.size())
        return {};
    return m_users.at(row);
}
