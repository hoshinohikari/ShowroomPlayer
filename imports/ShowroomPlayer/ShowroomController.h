#pragma once

#include "UserListModel.h"

#include <QObject>
#include <QSet>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class ShowroomApi;
class QTimer;

class ShowroomController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(UserListModel *users READ users CONSTANT)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(int pollIntervalMs READ pollIntervalMs WRITE setPollIntervalMs NOTIFY pollIntervalMsChanged)

public:
    explicit ShowroomController(QObject *parent = nullptr);
    ~ShowroomController() override = default;

    static ShowroomController *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    UserListModel *users() { return &m_users; }
    int selectedIndex() const { return m_selectedIndex; }
    int pollIntervalMs() const { return m_pollIntervalMs; }

    void setSelectedIndex(int index);
    void setPollIntervalMs(int intervalMs);

public slots:
    void addUser(const QString &username);
    void removeUser(int index);
    void selectUser(int index);

signals:
    void selectedIndexChanged();
    void pollIntervalMsChanged();
    void playStream(const QString &url);
    void errorOccurred(const QString &message);

private slots:
    void pollOnlineRooms();
    void startPollingIfNeeded();

private:
    void resolveRoomId(const QString &username);
    void refreshLiveStatus(const QSet<qint64> &liveRoomIds);
    void fetchStreamUrl(qint64 roomId, const QString &username);
    void playSelectedUserIfLive();

    UserListModel m_users;
    ShowroomApi *m_api;
    QTimer *m_pollTimer;
    int m_selectedIndex = -1;
    int m_pollIntervalMs = 20000;
    bool m_pollInFlight = false;
};
