#include "ShowroomLiveSocket.h"
#include "ShowroomApi.h"
#include "ShowroomLog.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QAbstractSocket>
#include <QLatin1StringView>
#include <QUrl>
#include <QUrlQuery>
#include <QWebSocket>

namespace {

constexpr QLatin1StringView kWsUrl("wss://online.showroom-live.com/");
constexpr QLatin1StringView kOrigin("https://www.showroom-live.com");
constexpr auto kUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/151.0.0.0 Safari/537.36";
constexpr int kLiveStatusOnAir = 2;

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
{
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

    if (m_roomId == roomId && (m_connected || m_socket->state() == QAbstractSocket::ConnectingState)) {
        qCDebug(lcShowroomLive) << "WebSocket already active for room" << roomId;
        return;
    }

    disconnectFromRoom();
    m_roomId = roomId;
    fetchLiveInfo(roomId);
}

void ShowroomLiveSocket::disconnectFromRoom()
{
    const qint64 previousRoom = m_roomId;
    m_bcsvrKey.clear();
    m_connected = false;
    m_roomId = 0;

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        qCInfo(lcShowroomLive) << "Closing WebSocket for room" << previousRoom;
        m_socket->close();
    }
}

void ShowroomLiveSocket::fetchLiveInfo(qint64 roomId)
{
    qCInfo(lcShowroomLive) << "Fetching live_info for WebSocket, room:" << roomId;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("room_id"), QString::number(roomId));

    m_api->get(QStringLiteral("api/live/live_info"), query, [this, roomId](QNetworkReply *reply) {
        if (m_roomId != roomId)
            return;

        if (reply->error() != QNetworkReply::NoError) {
            const QString message =
                tr("live_info request failed: %1").arg(reply->errorString());
            qCWarning(lcShowroomLive) << message;
            emit socketError(message);
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        if (!document.isObject()) {
            qCWarning(lcShowroomLive) << "live_info returned invalid JSON for room" << roomId;
            emit socketError(tr("Invalid live_info response"));
            return;
        }

        const QJsonObject root = document.object();
        const int liveStatus = root.value(QLatin1String("live_status")).toInt(0);
        const QString bcsvrKey = root.value(QLatin1String("bcsvr_key")).toString().trimmed();

        if (liveStatus != kLiveStatusOnAir || bcsvrKey.isEmpty()) {
            qCInfo(lcShowroomLive) << "Room" << roomId << "not on air for WebSocket, status:"
                                   << liveStatus << "bcsvr_key empty:" << bcsvrKey.isEmpty();
            return;
        }

        qCInfo(lcShowroomLive) << "live_info ready, bcsvr_key:" << bcsvrKey;
        openWebSocket(bcsvrKey);
    });
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
    sendSubscribe();
    emit connected(m_roomId);
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
    if (m_connected) {
        qCInfo(lcShowroomLive) << "WebSocket disconnected";
        emit disconnected();
    }
    m_connected = false;
}

void ShowroomLiveSocket::onSocketError()
{
    const QString message = m_socket->errorString();
    qCWarning(lcShowroomLive) << "WebSocket error:" << message;
    emit socketError(message);
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
