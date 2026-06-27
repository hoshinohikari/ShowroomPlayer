#pragma once

#include <QJsonObject>
#include <QObject>

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
    void disconnected();
    void commentReceived(const QJsonObject &payload);
    void socketError(const QString &message);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError();
    void onTextMessageReceived(const QString &message);

private:
    void fetchLiveInfo(qint64 roomId);
    void openWebSocket(const QString &bcsvrKey);
    void sendSubscribe();
    void handleServerMessage(const QString &message);

    ShowroomApi *m_api;
    QWebSocket *m_socket = nullptr;
    QString m_bcsvrKey;
    qint64 m_roomId = 0;
    bool m_connected = false;
};
