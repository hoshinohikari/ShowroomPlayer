#pragma once

#include "UserListModel.h"
#include "LiveChatModel.h"
#include "LiveGiftModel.h"

#include <QJsonArray>
#include <QObject>
#include <QSet>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class ShowroomApi;
class QTimer;
class ShowroomAuth;
class ShowroomLiveSocket;

class ShowroomController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(UserListModel *users READ users CONSTANT)
    Q_PROPERTY(LiveChatModel *liveChat READ liveChat CONSTANT)
    Q_PROPERTY(LiveGiftModel *liveGifts READ liveGifts CONSTANT)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(int pollIntervalMs READ pollIntervalMs WRITE setPollIntervalMs NOTIFY pollIntervalMsChanged)

public:
    explicit ShowroomController(QObject *parent = nullptr);
    ~ShowroomController() override = default;

    static ShowroomController *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    UserListModel *users() { return &m_users; }
    LiveChatModel *liveChat() { return &m_liveChat; }
    LiveGiftModel *liveGifts() { return &m_liveGifts; }
    int selectedIndex() const { return m_selectedIndex; }
    int pollIntervalMs() const { return m_pollIntervalMs; }

    void setSelectedIndex(int index);
    void setPollIntervalMs(int intervalMs);

public slots:
    void addUser(const QString &username);
    void removeUser(int index);
    void selectUser(int index);
    void wireToAuth(ShowroomAuth *auth);
    void ensureMonitoring();

signals:
    void selectedIndexChanged();
    void pollIntervalMsChanged();
    void playStream(const QString &url);
    void errorOccurred(const QString &message);

private slots:
    void pollOnlineRooms();
    void startPollingIfNeeded();
    void onLoggedInChanged();
    void ingestLiveMessage(const QJsonObject &payload);

private:
    void resolveRoomId(const QString &username);
    void refreshLiveStatus(const QSet<qint64> &liveRoomIds);
    void syncFollowerMonitors(const QJsonArray &rooms, QSet<qint64> *liveRoomIds);
    void fetchStreamUrl(qint64 roomId, const QString &username);
    void playSelectedUserIfLive();
    void startLiveSocket(qint64 roomId);
    void stopLiveSocket();
    void fetchGiftList(qint64 roomId);
    void fetchEventStatus(qint64 roomId);
    void finishPoll(const QSet<qint64> &liveRoomIds);
    void ensureAuth();
    void wireAuth();
    void connectAuthSignals();
    bool isLoggedIn() const;

    UserListModel m_users;
    LiveChatModel m_liveChat;
    LiveGiftModel m_liveGifts;
    ShowroomApi *m_api;
    QQmlEngine *m_qmlEngine = nullptr;
    ShowroomAuth *m_auth = nullptr;
    QTimer *m_pollTimer;
    int m_selectedIndex = -1;
    int m_pollIntervalMs = 20000;
    bool m_pollInFlight = false;
    bool m_pollPending = false;
    bool m_authWired = false;
    qint64 m_fetchingStreamRoomId = -1;
    QSet<QString> m_dismissedFollowers;
    ShowroomLiveSocket *m_liveSocket = nullptr;
};
