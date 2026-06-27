#include "ShowroomController.h"
#include "ShowroomApi.h"
#include "ShowroomAuth.h"
#include "ShowroomLiveSocket.h"
#include "ShowroomLog.h"

#include <QDateTime>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QtQml/qqml.h>
#include <QMetaObject>
#include <QNetworkReply>
#include <QTimer>
#include <QUrlQuery>

namespace {

qint64 parseRoomId(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isDouble())
        return static_cast<qint64>(value.toDouble());
    if (value.isString()) {
        bool ok = false;
        const qint64 id = value.toString().toLongLong(&ok);
        if (ok)
            return id;
    }
    return 0;
}

QString parseHlsUrl(const QByteArray &payload, int *selectedQuality = nullptr)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject())
        return {};

    const QJsonValue streamsValue = document.object().value(QLatin1String("streaming_url_list"));
    if (!streamsValue.isArray())
        return {};

    QString hlsAllUrl;
    QString bestHlsUrl;
    int bestQuality = -1;

    for (const QJsonValue &streamValue : streamsValue.toArray()) {
        if (!streamValue.isObject())
            continue;

        const QJsonObject stream = streamValue.toObject();
        const QString type = stream.value(QLatin1String("type")).toString();
        const QString url = stream.value(QLatin1String("url")).toString();
        if (url.isEmpty())
            continue;

        if (type == QLatin1String("hls_all")) {
            if (hlsAllUrl.isEmpty())
                hlsAllUrl = url;
            continue;
        }

        if (type != QLatin1String("hls"))
            continue;

        const int quality = stream.value(QLatin1String("quality")).toInt(-1);
        if (quality > bestQuality) {
            bestQuality = quality;
            bestHlsUrl = url;
        }
    }

    if (!bestHlsUrl.isEmpty()) {
        if (selectedQuality)
            *selectedQuality = bestQuality;
        return bestHlsUrl;
    }

    if (selectedQuality)
        *selectedQuality = 0;
    return hlsAllUrl;
}

QSet<qint64> parseLiveRoomIds(const QByteArray &payload)
{
    QSet<qint64> roomIds;
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject())
        return roomIds;

    const QJsonValue onlivesValue = document.object().value(QLatin1String("onlives"));
    if (!onlivesValue.isArray())
        return roomIds;

    for (const QJsonValue &genreValue : onlivesValue.toArray()) {
        if (!genreValue.isObject())
            continue;

        const QJsonArray lives = genreValue.toObject().value(QLatin1String("lives")).toArray();
        for (const QJsonValue &liveValue : lives) {
            if (!liveValue.isObject())
                continue;

            const qint64 roomId = parseRoomId(liveValue.toObject(), "room_id");
            if (roomId > 0)
                roomIds.insert(roomId);
        }
    }

    return roomIds;
}

} // namespace

ShowroomController::ShowroomController(QObject *parent)
    : QObject(parent)
    , m_api(ShowroomApi::shared(this))
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(m_pollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &ShowroomController::pollOnlineRooms);
    m_liveSocket = new ShowroomLiveSocket(m_api, this);
    connect(m_liveSocket, &ShowroomLiveSocket::commentReceived, &m_liveChat,
            &LiveChatModel::ingestPayload);
    connect(m_liveSocket, &ShowroomLiveSocket::disconnected, &m_liveChat, &LiveChatModel::clear);
    connect(m_liveSocket, &ShowroomLiveSocket::connected, &m_liveChat, &LiveChatModel::clear);
    qCInfo(lcShowroomController) << "Monitor ready, waiting for session check before polling";
    QTimer::singleShot(0, this, &ShowroomController::wireAuth);
}

void ShowroomController::ensureAuth()
{
    if (m_auth)
        return;

    if (m_qmlEngine) {
        m_auth = m_qmlEngine->singletonInstance<ShowroomAuth *>(
            QStringLiteral("ShowroomPlayer"), QStringLiteral("ShowroomAuth"));
    }

    if (!m_auth)
        m_auth = ShowroomAuth::instance();
}

bool ShowroomController::isLoggedIn() const
{
    return m_auth && m_auth->loggedIn();
}

void ShowroomController::startPollingIfNeeded()
{
    if (!m_pollTimer->isActive())
        m_pollTimer->start();

    if (m_pollInFlight) {
        m_pollPending = true;
        return;
    }

    qCInfo(lcShowroomController) << "Monitor started, poll interval:" << m_pollIntervalMs << "ms";
    QMetaObject::invokeMethod(this, &ShowroomController::pollOnlineRooms, Qt::QueuedConnection);
}

void ShowroomController::ensureMonitoring()
{
    startPollingIfNeeded();
}

void ShowroomController::connectAuthSignals()
{
    if (m_authWired)
        return;
    m_authWired = true;

    QObject::connect(m_auth, &ShowroomAuth::sessionCheckFinished, this,
                     &ShowroomController::startPollingIfNeeded);
    QObject::connect(m_auth, &ShowroomAuth::loginSucceeded, this,
                     &ShowroomController::pollOnlineRooms);
    QObject::connect(m_auth, &ShowroomAuth::loggedInChanged, this,
                     &ShowroomController::onLoggedInChanged);
}

void ShowroomController::wireToAuth(ShowroomAuth *auth)
{
    if (!auth) {
        qCWarning(lcShowroomController) << "wireToAuth called with null auth";
        return;
    }

    m_auth = auth;

    if (m_authWired)
        return;

    connectAuthSignals();
    qCInfo(lcShowroomController) << "Auth wiring complete";

    if (m_auth->sessionCheckDone())
        startPollingIfNeeded();
}

void ShowroomController::wireAuth()
{
    if (!m_qmlEngine) {
        QTimer::singleShot(0, this, &ShowroomController::wireAuth);
        return;
    }

    ShowroomAuth *auth = m_qmlEngine->singletonInstance<ShowroomAuth *>(
        QStringLiteral("ShowroomPlayer"), QStringLiteral("ShowroomAuth"));
    if (!auth) {
        QTimer::singleShot(0, this, &ShowroomController::wireAuth);
        return;
    }

    wireToAuth(auth);
}

void ShowroomController::onLoggedInChanged()
{
    if (!isLoggedIn())
        m_dismissedFollowers.clear();
}

ShowroomController *ShowroomController::create(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(scriptEngine)
    static ShowroomController instance;
    if (engine)
        instance.m_qmlEngine = engine;
    else if (scriptEngine)
        instance.m_qmlEngine = qmlEngine(scriptEngine);
    QQmlEngine::setObjectOwnership(&instance, QQmlEngine::CppOwnership);
    return &instance;
}

void ShowroomController::setSelectedIndex(int index)
{
    if (m_selectedIndex == index)
        return;

    stopLiveSocket();

    const QString previous = m_selectedIndex >= 0 && m_selectedIndex < m_users.rowCount()
                                 ? m_users.userAt(m_selectedIndex).username
                                 : QString();
    m_selectedIndex = index;

    const QString current = m_selectedIndex >= 0 && m_selectedIndex < m_users.rowCount()
                                ? m_users.userAt(m_selectedIndex).username
                                : QString();
    qCInfo(lcShowroomController) << "Selected user changed:"
                                 << (previous.isEmpty() ? QStringLiteral("-") : previous)
                                 << "->" << (current.isEmpty() ? QStringLiteral("-") : current);

    emit selectedIndexChanged();
    playSelectedUserIfLive();
}

void ShowroomController::setPollIntervalMs(int intervalMs)
{
    const int clamped = qMax(5000, intervalMs);
    if (m_pollIntervalMs == clamped)
        return;

    m_pollIntervalMs = clamped;
    m_pollTimer->setInterval(m_pollIntervalMs);
    qCInfo(lcShowroomController) << "Poll interval updated to" << m_pollIntervalMs << "ms";
    emit pollIntervalMsChanged();
}

void ShowroomController::addUser(const QString &username)
{
    const QString trimmed = username.trimmed();
    if (trimmed.isEmpty()) {
        qCWarning(lcShowroomController) << "Reject add user: empty username";
        emit errorOccurred(tr("Username cannot be empty"));
        return;
    }

    if (m_users.containsUsername(trimmed)) {
        qCWarning(lcShowroomController) << "Reject add user: already monitoring" << trimmed;
        emit errorOccurred(tr("User already in monitor list"));
        return;
    }

    m_dismissedFollowers.remove(trimmed.toLower());

    const int row = m_users.appendUser(trimmed, 0);
    qCInfo(lcShowroomController) << "Added user to monitor list:" << trimmed << "row:" << row;

    if (m_selectedIndex < 0)
        setSelectedIndex(row);

    resolveRoomId(trimmed);
    pollOnlineRooms();
}

void ShowroomController::removeUser(int index)
{
    if (index < 0 || index >= m_users.rowCount())
        return;

    const QString username = m_users.userAt(index).username;
    m_dismissedFollowers.insert(username.trimmed().toLower());
    m_users.removeAt(index);
    qCInfo(lcShowroomController) << "Removed user from monitor list:" << username << "row:" << index;

    if (m_users.rowCount() == 0) {
        setSelectedIndex(-1);
        return;
    }

    if (m_selectedIndex == index)
        setSelectedIndex(qMin(index, m_users.rowCount() - 1));
    else if (m_selectedIndex > index)
        setSelectedIndex(m_selectedIndex - 1);
}

void ShowroomController::selectUser(int index)
{
    if (index < 0 || index >= m_users.rowCount()) {
        qCWarning(lcShowroomController) << "Reject select user: invalid index" << index;
        return;
    }

    if (m_selectedIndex == index) {
        qCInfo(lcShowroomController) << "Resuming playback for" << m_users.userAt(index).username;
        playSelectedUserIfLive();
        return;
    }

    setSelectedIndex(index);
}

void ShowroomController::resolveRoomId(const QString &username)
{
    qCDebug(lcShowroomController) << "Resolving room id for" << username;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("room_url_key"), username);

    m_api->get(QStringLiteral("api/room/status"), query, [this, username](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            qCCritical(lcShowroomController) << "Resolve room failed for" << username << ":"
                                             << reply->errorString();
            emit errorOccurred(tr("Failed to resolve room for %1").arg(username));
            return;
        }

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 404) {
            qCWarning(lcShowroomController) << "User not found:" << username;
            emit errorOccurred(tr("User not found: %1").arg(username));
            return;
        }

        const qint64 roomId = parseRoomId(
            QJsonDocument::fromJson(reply->readAll()).object(), "room_id");
        const int row = m_users.indexOfUsername(username);
        if (row < 0 || roomId <= 0) {
            if (row >= 0 && roomId <= 0) {
                qCCritical(lcShowroomController) << "Invalid room id for" << username;
                emit errorOccurred(tr("Invalid room id for %1").arg(username));
            }
            return;
        }

        qCInfo(lcShowroomController) << "Resolved room id for" << username << ":" << roomId;
        m_users.setRoomId(row, roomId);
        pollOnlineRooms();
    });
}

void ShowroomController::syncFollowerMonitors(const QJsonArray &rooms, QSet<qint64> *liveRoomIds)
{
    for (const QJsonValue &roomValue : rooms) {
        if (!roomValue.isObject())
            continue;

        const QJsonObject room = roomValue.toObject();
        const QString username = room.value(QLatin1String("room_url_key")).toString().trimmed();
        const qint64 roomId = parseRoomId(room, "room_id");
        const bool isOnline = room.value(QLatin1String("is_online")).toInt(0) != 0;

        if (!isOnline || username.isEmpty() || roomId <= 0)
            continue;

        if (m_dismissedFollowers.contains(username.toLower()))
            continue;

        if (liveRoomIds)
            liveRoomIds->insert(roomId);

        if (m_users.containsUsername(username))
            continue;

        const int row = m_users.appendUser(username, roomId);
        m_users.setLive(row, true);
        qCInfo(lcShowroomController) << "Auto-added live follower:" << username << "room:" << roomId;

        if (m_selectedIndex < 0)
            setSelectedIndex(row);
    }
}

void ShowroomController::finishPoll(const QSet<qint64> &liveRoomIds)
{
    m_pollInFlight = false;
    refreshLiveStatus(liveRoomIds);

    if (m_pollPending) {
        m_pollPending = false;
        QMetaObject::invokeMethod(this, &ShowroomController::pollOnlineRooms, Qt::QueuedConnection);
    }
}

void ShowroomController::pollOnlineRooms()
{
    if (m_pollInFlight) {
        m_pollPending = true;
        qCDebug(lcShowroomController) << "Skip poll: previous request still in flight";
        return;
    }

    ensureAuth();
    const bool loggedIn = isLoggedIn();
    if (m_users.rowCount() == 0 && !loggedIn) {
        qCDebug(lcShowroomController) << "Skip poll: monitor list is empty and not logged in";
        return;
    }

    qCInfo(lcShowroomController) << "Polling online rooms, monitored users:" << m_users.rowCount()
                                  << "logged in:" << loggedIn;
    m_pollInFlight = true;
    m_api->get(QStringLiteral("api/live/onlives"), QUrlQuery(), [this](QNetworkReply *reply) {
        QSet<qint64> liveRoomIds;
        if (reply->error() == QNetworkReply::NoError)
            liveRoomIds = parseLiveRoomIds(reply->readAll());
        else
            qCCritical(lcShowroomController) << "Poll online rooms failed:" << reply->errorString();

        qCDebug(lcShowroomController) << "Online room count from API:" << liveRoomIds.size();

        if (!isLoggedIn()) {
            if (reply->error() != QNetworkReply::NoError)
                emit errorOccurred(tr("Failed to fetch online rooms"));
            finishPoll(liveRoomIds);
            return;
        }

        qCInfo(lcShowroomController) << "Fetching follow rooms for logged-in user";
        m_api->get(QStringLiteral("api/follow/rooms"), QUrlQuery(),
                   [this, liveRoomIds](QNetworkReply *followReply) mutable {
                       QSet<qint64> mergedLiveRoomIds = liveRoomIds;

                       if (followReply->error() == QNetworkReply::NoError) {
                           const QJsonDocument document =
                               QJsonDocument::fromJson(followReply->readAll());
                           const QJsonArray rooms =
                               document.object().value(QLatin1String("rooms")).toArray();

                           syncFollowerMonitors(rooms, &mergedLiveRoomIds);
                           qCInfo(lcShowroomController) << "Follow rooms fetched, count:" << rooms.size();
                       } else {
                           qCWarning(lcShowroomController)
                               << "Fetch follow rooms failed:" << followReply->errorString();
                       }

                       finishPoll(mergedLiveRoomIds);
                   });
    });
}

void ShowroomController::refreshLiveStatus(const QSet<qint64> &liveRoomIds)
{
    bool selectedUserBecameLive = false;

    for (int row = 0; row < m_users.rowCount(); ++row) {
        const MonitoredUser user = m_users.userAt(row);
        if (user.roomId <= 0)
            continue;

        const bool live = liveRoomIds.contains(user.roomId);
        const bool wasLive = user.isLive;
        if (live != wasLive) {
            qCInfo(lcShowroomController) << "Live status changed for" << user.username
                                         << "room:" << user.roomId
                                         << (live ? "now live" : "now offline");
        }
        m_users.setLive(row, live);

        if (row == m_selectedIndex && live && !wasLive)
            selectedUserBecameLive = true;
    }

    if (selectedUserBecameLive) {
        qCInfo(lcShowroomController) << "Selected user went live, starting playback";
        playSelectedUserIfLive();
    } else if (m_selectedIndex >= 0 && m_selectedIndex < m_users.rowCount()) {
        const MonitoredUser selected = m_users.userAt(m_selectedIndex);
        if (!selected.isLive)
            stopLiveSocket();
    }
}

void ShowroomController::playSelectedUserIfLive()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_users.rowCount()) {
        qCDebug(lcShowroomController) << "Skip playback: no user selected";
        return;
    }

    const MonitoredUser user = m_users.userAt(m_selectedIndex);
    if (!user.isLive) {
        qCDebug(lcShowroomController) << "Skip playback: selected user is offline" << user.username;
        return;
    }

    if (user.roomId <= 0) {
        qCWarning(lcShowroomController) << "Skip playback: room id not resolved for" << user.username;
        return;
    }

    fetchStreamUrl(user.roomId, user.username);
}

void ShowroomController::startLiveSocket(qint64 roomId)
{
    if (!m_liveSocket)
        return;

    fetchGiftList(roomId);
    m_liveSocket->connectToRoom(roomId);
}

void ShowroomController::fetchGiftList(qint64 roomId)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("room_id"), QString::number(roomId));

    m_api->get(QStringLiteral("api/live/gift_list"), query,
               [this, roomId](QNetworkReply *reply) {
                   if (reply->error() != QNetworkReply::NoError) {
                       qCWarning(lcShowroomController) << "Gift list fetch failed for room"
                                                         << roomId << ":" << reply->errorString();
                       return;
                   }

                   if (m_selectedIndex < 0)
                       return;

                   const MonitoredUser selected = m_users.userAt(m_selectedIndex);
                   if (selected.roomId != roomId) {
                       qCDebug(lcShowroomController)
                           << "Ignore gift list: selection changed away from room" << roomId;
                       return;
                   }

                   const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                   if (!doc.isObject()) {
                       qCWarning(lcShowroomController) << "Gift list response is not JSON object";
                       return;
                   }

                   m_liveChat.ingestGiftList(doc.object());
               });
}

void ShowroomController::stopLiveSocket()
{
    if (!m_liveSocket)
        return;

    m_liveSocket->disconnectFromRoom();
}

void ShowroomController::fetchStreamUrl(qint64 roomId, const QString &username)
{
    if (m_fetchingStreamRoomId == roomId) {
        qCDebug(lcShowroomController) << "Skip duplicate stream fetch for room:" << roomId;
        return;
    }

    m_fetchingStreamRoomId = roomId;
    qCInfo(lcShowroomController) << "Fetching stream URL for" << username << "room:" << roomId;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("room_id"), QString::number(roomId));
    query.addQueryItem(QStringLiteral("_"),
                       QString::number(QDateTime::currentMSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("abr_available"), QStringLiteral("1"));

    m_api->get(QStringLiteral("api/live/streaming_url"), query,
               [this, username, roomId](QNetworkReply *reply) {
                   if (m_fetchingStreamRoomId == roomId)
                       m_fetchingStreamRoomId = -1;

                   if (reply->error() != QNetworkReply::NoError) {
                       qCCritical(lcShowroomController) << "Fetch stream failed for" << username
                                                        << ":" << reply->errorString();
                       emit errorOccurred(tr("Failed to fetch stream for %1").arg(username));
                       return;
                   }

                   int selectedQuality = -1;
                   const QString streamUrl = parseHlsUrl(reply->readAll(), &selectedQuality);
                   if (streamUrl.isEmpty()) {
                       qCWarning(lcShowroomController) << "No HLS stream in response for" << username;
                       emit errorOccurred(tr("No HLS stream for %1").arg(username));
                       return;
                   }

                   if (m_selectedIndex < 0)
                       return;

                   const MonitoredUser selected = m_users.userAt(m_selectedIndex);
                   if (selected.roomId != roomId) {
                       qCDebug(lcShowroomController)
                           << "Ignore stream response: selection changed away from" << username;
                       return;
                   }

                   qCInfo(lcShowroomController) << "Stream ready for" << username
                                                << "quality:" << selectedQuality
                                                << "url:" << streamUrl;
                   emit playStream(streamUrl);
                   startLiveSocket(roomId);
               });
}
