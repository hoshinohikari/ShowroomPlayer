#pragma once

#include <QAbstractListModel>
#include <QString>

struct MonitoredUser
{
    QString username;
    qint64 roomId = 0;
    bool isLive = false;
};

class UserListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        UsernameRole = Qt::UserRole + 1,
        RoomIdRole,
        IsLiveRole,
    };

    explicit UserListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool containsUsername(const QString &username) const;
    int indexOfUsername(const QString &username) const;
    int appendUser(const QString &username, qint64 roomId);
    void removeAt(int row);
    void setLive(int row, bool live);
    void setRoomId(int row, qint64 roomId);
    MonitoredUser userAt(int row) const;

private:
    QList<MonitoredUser> m_users;
};
