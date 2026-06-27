#pragma once

#include <QJsonObject>
#include <QObject>

class QTimer;
class QWebSocket;
class ShowroomApi;

class ShowroomLiveSocket : public QObject
{
    Q_OBJECT

public:
    explicit ShowroomLiveSocket(ShowroomApi *api, QObject *parent = nullptr);
    ~ShowroomLiveSocket() override;

    bool isConnected() const { return m_connected; }
    qint64 roomId() const { return m_roomId; }

public slots:
    void connectToRoom(qint64 roomId);
    void disconnectFromRoom();

signals:
    void connected(qint64 roomId);
    void reconnected(qint64 roomId);
    void disconnected();
    void sessionEnded(qint64 roomId);
    void commentReceived(const QJsonObject &payload);
    void socketError(const QString &message);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError();
    void onTextMessageReceived(const QString &message);
    void onReconnectTimer();

private:
    void closeSocket();
    void fetchLiveInfo(qint64 roomId, bool resumeSession);
    void beginReconnect();
    void scheduleReconnect();
    void openWebSocket(const QString &bcsvrKey);
    void sendSubscribe();
    void handleServerMessage(const QString &message);
    void handleLiveInfoReady(qint64 roomId, const QString &bcsvrKey, bool resumeSession);
    void handleLiveInfoOffline(qint64 roomId, int liveStatus, bool duringReconnect);

    ShowroomApi *m_api;
    QWebSocket *m_socket = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QString m_bcsvrKey;
    qint64 m_roomId = 0;
    int m_reconnectAttempt = 0;
    bool m_connected = false;
    bool m_wantsConnection = false;
    bool m_userDisconnect = false;
    bool m_resumeSession = false;

    static constexpr int kLiveStatusOnAir = 2;
    static constexpr int kInitialReconnectDelayMs = 2000;
    static constexpr int kMaxReconnectDelayMs = 30000;
};
