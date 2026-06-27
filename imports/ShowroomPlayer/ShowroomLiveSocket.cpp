#include "ShowroomLiveSocket.h"
#include "ShowroomApi.h"
#include "ShowroomLog.h"

#include <QAbstractSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QWebSocket>
#include <QtMath>

namespace {

constexpr QLatin1StringView kWsUrl("wss://online.showroom-live.com/");
constexpr QLatin1StringView kOrigin("https://www.showroom-live.com");
constexpr auto kUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/151.0.0.0 Safari/537.36";

QByteArray buildCookieHeader(ShowroomApi *api)
{
    QList<QNetworkCookie> cookies =
        api->cookiesForUrl(QUrl(QStringLiteral("https://online.showroom-live.com/")));
    if (cookies.isEmpty())
        cookies = api->cookiesForUrl(QUrl(kOrigin));
    if (cookies.isEmpty())
        return {};

    QByteArray header;
    for (const QNetworkCookie &cookie : cookies) {
        if (!header.isEmpty())
            header.append("; ");
        header.append(cookie.name());
        header.append('=');
        header.append(cookie.value());
    }
    return header;
}

} // namespace

ShowroomLiveSocket::ShowroomLiveSocket(ShowroomApi *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_reconnectTimer(new QTimer(this))
{
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ShowroomLiveSocket::onReconnectTimer);

    connect(m_socket, &QWebSocket::connected, this, &ShowroomLiveSocket::onSocketConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &ShowroomLiveSocket::onSocketDisconnected);
    connect(m_socket, &QWebSocket::errorOccurred, this, &ShowroomLiveSocket::onSocketError);
    connect(m_socket, &QWebSocket::textMessageReceived, this,
            &ShowroomLiveSocket::onTextMessageReceived);
}

ShowroomLiveSocket::~ShowroomLiveSocket()
{
    disconnectFromRoom();
}

void ShowroomLiveSocket::connectToRoom(qint64 roomId)
{
    if (roomId <= 0) {
        qCWarning(lcShowroomLive) << "Reject WebSocket connect: invalid room id";
        return;
    }

    if (m_roomId == roomId
        && (m_connected || m_socket->state() == QAbstractSocket::ConnectingState)) {
        qCDebug(lcShowroomLive) << "WebSocket already active for room" << roomId;
        return;
    }

    closeSocket();
    m_reconnectTimer->stop();
    m_userDisconnect = false;
    m_wantsConnection = true;
    m_reconnectAttempt = 0;
    m_connected = false;
    m_bcsvrKey.clear();
    m_roomId = roomId;

    fetchLiveInfo(roomId, false);
}

void ShowroomLiveSocket::disconnectFromRoom()
{
    m_userDisconnect = true;
    m_wantsConnection = false;
    m_reconnectTimer->stop();

    const qint64 previousRoom = m_roomId;
    m_bcsvrKey.clear();
    m_connected = false;
    m_roomId = 0;
    m_reconnectAttempt = 0;

    closeSocket();

    if (previousRoom > 0) {
        qCInfo(lcShowroomLive) << "WebSocket closed by user for room" << previousRoom;
        emit disconnected();
    }
}

void ShowroomLiveSocket::closeSocket()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->close();
}

void ShowroomLiveSocket::fetchLiveInfo(qint64 roomId, bool resumeSession)
{
    qCInfo(lcShowroomLive) << (resumeSession ? "Revalidating live status for reconnect, room:"
                                               : "Fetching live_info for WebSocket, room:")
                           << roomId;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("room_id"), QString::number(roomId));

    m_api->get(QStringLiteral("api/live/live_info"), query,
               [this, roomId, resumeSession](QNetworkReply *reply) {
                   if (m_userDisconnect || !m_wantsConnection || m_roomId != roomId)
                       return;

                   if (reply->error() != QNetworkReply::NoError) {
                       const QString message =
                           tr("live_info request failed: %1").arg(reply->errorString());
                       qCWarning(lcShowroomLive) << message;
                       if (resumeSession) {
                           scheduleReconnect();
                           return;
                       }
                       emit socketError(message);
                       return;
                   }

                   const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
                   if (!document.isObject()) {
                       qCWarning(lcShowroomLive) << "live_info returned invalid JSON for room"
                                                 << roomId;
                       if (resumeSession)
                           scheduleReconnect();
                       else
                           emit socketError(tr("Invalid live_info response"));
                       return;
                   }

                   const QJsonObject root = document.object();
                   const int liveStatus = root.value(QLatin1String("live_status")).toInt(0);
                   const QString bcsvrKey = root.value(QLatin1String("bcsvr_key")).toString().trimmed();

                   if (liveStatus != kLiveStatusOnAir || bcsvrKey.isEmpty()) {
                       handleLiveInfoOffline(roomId, liveStatus, resumeSession);
                       return;
                   }

                   handleLiveInfoReady(roomId, bcsvrKey, resumeSession);
               });
}

void ShowroomLiveSocket::handleLiveInfoReady(qint64 roomId, const QString &bcsvrKey,
                                             bool resumeSession)
{
    if (m_userDisconnect || !m_wantsConnection || m_roomId != roomId)
        return;

    qCInfo(lcShowroomLive) << (resumeSession ? "Reconnect live_info ready, bcsvr_key:"
                                             : "live_info ready, bcsvr_key:")
                           << bcsvrKey;
    m_resumeSession = resumeSession;
    openWebSocket(bcsvrKey);
}

void ShowroomLiveSocket::handleLiveInfoOffline(qint64 roomId, int liveStatus, bool duringReconnect)
{
    if (m_userDisconnect || m_roomId != roomId)
        return;

    qCInfo(lcShowroomLive) << "Room" << roomId << "not on air, status:" << liveStatus
                           << "during reconnect:" << duringReconnect;

    m_wantsConnection = false;
    m_roomId = 0;
    m_bcsvrKey.clear();
    m_reconnectTimer->stop();
    emit sessionEnded(roomId);
}

void ShowroomLiveSocket::openWebSocket(const QString &bcsvrKey)
{
    m_bcsvrKey = bcsvrKey;

    const QUrl wsUrl(kWsUrl);
    QNetworkRequest request{wsUrl};
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Origin", QByteArray(kOrigin));
    request.setRawHeader("Pragma", "no-cache");
    request.setRawHeader("Cache-Control", "no-cache");

    const QByteArray cookieHeader = buildCookieHeader(m_api);
    if (!cookieHeader.isEmpty())
        request.setRawHeader("Cookie", cookieHeader);

    qCInfo(lcShowroomLive) << "Upgrading to WebSocket at" << kWsUrl << "for room" << m_roomId;
    m_socket->open(request);
}

void ShowroomLiveSocket::onSocketConnected()
{
    qCInfo(lcShowroomLive) << "WebSocket connected (101 Switching Protocols), room:" << m_roomId;
    m_connected = true;
    m_reconnectAttempt = 0;
    sendSubscribe();

    if (m_resumeSession) {
        qCInfo(lcShowroomLive) << "WebSocket session resumed for room" << m_roomId;
        emit reconnected(m_roomId);
        m_resumeSession = false;
    } else {
        emit connected(m_roomId);
    }
}

void ShowroomLiveSocket::sendSubscribe()
{
    if (m_bcsvrKey.isEmpty() || m_socket->state() != QAbstractSocket::ConnectedState)
        return;

    const QString payload = QStringLiteral("SUB\t") + m_bcsvrKey;
    qCInfo(lcShowroomLive) << "Sending SUB for room" << m_roomId;
    m_socket->sendTextMessage(payload);
}

void ShowroomLiveSocket::onSocketDisconnected()
{
    const bool wasConnected = m_connected;
    m_connected = false;

    if (m_userDisconnect || !m_wantsConnection || m_roomId <= 0)
        return;

    qCInfo(lcShowroomLive) << "WebSocket unexpectedly disconnected from room" << m_roomId
                           << "was connected:" << wasConnected;
    beginReconnect();
}

void ShowroomLiveSocket::onSocketError()
{
    const QString message = m_socket->errorString();
    qCWarning(lcShowroomLive) << "WebSocket error for room" << m_roomId << ":" << message;

    if (!m_userDisconnect && m_wantsConnection && m_roomId > 0)
        emit socketError(message);
}

void ShowroomLiveSocket::beginReconnect()
{
    if (m_userDisconnect || !m_wantsConnection || m_roomId <= 0)
        return;

    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        closeSocket();

    scheduleReconnect();
}

void ShowroomLiveSocket::scheduleReconnect()
{
    if (m_userDisconnect || !m_wantsConnection || m_roomId <= 0)
        return;

    const int delay = m_reconnectAttempt == 0
                          ? 0
                          : qMin(kInitialReconnectDelayMs
                                         * (1 << qMin(m_reconnectAttempt - 1, 4)),
                                 kMaxReconnectDelayMs);

    qCInfo(lcShowroomLive) << "Scheduling WebSocket reconnect in" << delay << "ms for room"
                           << m_roomId << "attempt:" << (m_reconnectAttempt + 1);
    m_reconnectTimer->start(delay);
}

void ShowroomLiveSocket::onReconnectTimer()
{
    if (m_userDisconnect || !m_wantsConnection || m_roomId <= 0)
        return;

    ++m_reconnectAttempt;
    qCInfo(lcShowroomLive) << "Reconnect attempt" << m_reconnectAttempt << "for room" << m_roomId;
    fetchLiveInfo(m_roomId, true);
}

void ShowroomLiveSocket::onTextMessageReceived(const QString &message)
{
    handleServerMessage(message);
}

void ShowroomLiveSocket::handleServerMessage(const QString &message)
{
    if (message == QLatin1String("ACK\tshowroom") || message == QLatin1String("ACK showroom")) {
        qCInfo(lcShowroomLive) << "WebSocket ACK received for room" << m_roomId;
        return;
    }

    if (message.startsWith(QLatin1String("MSG"))) {
        const int jsonStart = message.indexOf(QLatin1Char('{'));
        if (jsonStart < 0) {
            qCDebug(lcShowroomLive) << "MSG without JSON payload:" << message.left(120);
            return;
        }

        const QByteArray jsonBytes = message.mid(jsonStart).toUtf8();
        const QJsonDocument document = QJsonDocument::fromJson(jsonBytes);
        if (!document.isObject()) {
            qCDebug(lcShowroomLive) << "Non-JSON MSG frame:" << message.left(120);
            return;
        }

        const QJsonObject payload = document.object();
        qCDebug(lcShowroomLive) << "Live message for room" << m_roomId << ":" << payload;
        emit commentReceived(payload);
        return;
    }

    if (message.contains(QLatin1String("UTF-8"))) {
        qCWarning(lcShowroomLive) << "WebSocket decode error:" << message;
        return;
    }

    qCDebug(lcShowroomLive) << "WebSocket text:" << message.left(120);
}
