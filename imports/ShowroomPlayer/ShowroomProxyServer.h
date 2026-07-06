#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QTcpServer;
class QNetworkAccessManager;
class QQmlEngine;
class QJSEngine;

class ShowroomProxyServer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(quint16 localPort READ localPort NOTIFY localPortChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString userAgent READ userAgent WRITE setUserAgent NOTIFY userAgentChanged)

public:
    explicit ShowroomProxyServer(QObject *parent = nullptr);
    ~ShowroomProxyServer() override;

    static ShowroomProxyServer *create(QQmlEngine *engine, QJSEngine *scriptEngine);
    static ShowroomProxyServer *instance();

    quint16 localPort() const { return m_localPort; }
    bool running() const { return m_running; }
    QString userAgent() const { return m_userAgent; }
    void setUserAgent(const QString &ua);

    Q_INVOKABLE QString rewriteUrl(const QString &url) const;
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

signals:
    void localPortChanged();
    void runningChanged();
    void userAgentChanged();

private:
    void onNewConnection();
    void handleConnection(class QTcpSocket *socket);
    void serveUpstream(class QTcpSocket *socket, const QUrl &realUrl, const QByteArray &rangeHeader);
    QByteArray rewritePlaylist(const QByteArray &data, const QUrl &playlistUrl) const;
    QString rewriteUrlString(const QString &maybeUrl, const QUrl &baseUrl) const;
    class QNetworkRequest buildUpstreamRequest(const QUrl &url, const QByteArray &rangeHeader) const;

    void setRunning(bool running);
    void setLocalPort(quint16 port);

    static ShowroomProxyServer *s_instance;

    QTcpServer *m_server = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    quint16 m_localPort = 0;
    bool m_running = false;
    QString m_userAgent;
};
