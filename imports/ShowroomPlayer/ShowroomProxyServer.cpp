#include "ShowroomProxyServer.h"
#include "ShowroomLog.h"

#include <QQmlEngine>

#include <QByteArray>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

namespace {

constexpr auto kChromeUserAgent =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

constexpr auto kOrigin = "https://www.showroom-live.com";
constexpr auto kReferer = "https://www.showroom-live.com/";

QByteArray encodeUrlComponent(const QUrl &url)
{
    return url.toString(QUrl::FullyEncoded).toUtf8().toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QString extractExtensionFromUrl(const QUrl &url)
{
    const QString path = url.path();
    const int lastDot = path.lastIndexOf('.');
    const int lastSlash = path.lastIndexOf('/');
    if (lastDot <= lastSlash)
        return QStringLiteral("ts");

    QString ext = path.mid(lastDot + 1).toLower();
    const int nonAlpha = ext.indexOf(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    if (nonAlpha >= 0)
        ext = ext.left(nonAlpha);
    if (ext.isEmpty() || ext.size() > 8)
        return QStringLiteral("ts");
    return ext;
}

QUrl decodeUrlFromPath(const QString &path)
{
    if (!path.startsWith(QStringLiteral("/p/")))
        return {};
    const QString rest = path.mid(3);
    if (rest.isEmpty())
        return {};

    // Format: /p/<base64url>/segment.<ext>
    // The trailing component exists only to satisfy FFmpeg's HLS segment
    // extension whitelist check; it carries no information for the proxy.
    const int slash = rest.indexOf('/');
    const QString encoded = slash >= 0 ? rest.left(slash) : rest;
    if (encoded.isEmpty())
        return {};

    const QByteArray decoded = QByteArray::fromBase64(
        encoded.toUtf8(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QUrl(QString::fromUtf8(decoded));
}

bool isPlaylistContentType(const QByteArray &contentType)
{
    const QByteArray lower = contentType.toLower();
    return lower.contains("mpegurl") || lower.contains("m3u8") || lower.contains("x-mpegurl");
}

bool isPlaylistPath(const QUrl &url)
{
    const QString path = url.path().toLower();
    return path.endsWith(QStringLiteral(".m3u8")) || path.endsWith(QStringLiteral(".m3u"));
}

QByteArray extractHeaderValue(const QByteArray &rawHeaders, const QByteArray &name)
{
    for (const QByteArray &line : rawHeaders.split('\n')) {
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QByteArray key = line.left(colon).trimmed().toLower();
        if (key == name.toLower())
            return line.mid(colon + 1).trimmed();
    }
    return {};
}

} // namespace

ShowroomProxyServer *ShowroomProxyServer::s_instance = nullptr;

ShowroomProxyServer::ShowroomProxyServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_nam(new QNetworkAccessManager(this))
    , m_userAgent(QString::fromLatin1(kChromeUserAgent))
{
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    start();
}

ShowroomProxyServer::~ShowroomProxyServer() = default;

ShowroomProxyServer *ShowroomProxyServer::create(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    if (!s_instance)
        s_instance = new ShowroomProxyServer();
    QQmlEngine::setObjectOwnership(s_instance, QQmlEngine::CppOwnership);
    return s_instance;
}

ShowroomProxyServer *ShowroomProxyServer::instance()
{
    if (!s_instance)
        s_instance = new ShowroomProxyServer();
    return s_instance;
}

void ShowroomProxyServer::setUserAgent(const QString &ua)
{
    const QString trimmed = ua.trimmed();
    const QString effective = trimmed.isEmpty() ? QString::fromLatin1(kChromeUserAgent) : trimmed;
    if (m_userAgent == effective)
        return;
    m_userAgent = effective;
    emit userAgentChanged();
}

QString ShowroomProxyServer::rewriteUrl(const QString &url) const
{
    const QUrl parsed(url);
    if (!parsed.isValid() || parsed.scheme() == QStringLiteral("http"))
        return url;

    if (!m_running || m_localPort == 0)
        return url;

    const QString ext = extractExtensionFromUrl(parsed);
    return QStringLiteral("http://127.0.0.1:%1/p/%2/segment.%3")
        .arg(m_localPort)
        .arg(QString::fromLatin1(encodeUrlComponent(parsed)))
        .arg(ext);
}

void ShowroomProxyServer::start()
{
    if (m_running)
        return;

    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        qCWarning(lcShowroomProxy) << "Failed to bind local proxy server:"
                                   << m_server->errorString();
        return;
    }

    setLocalPort(m_server->serverPort());
    setRunning(true);

    connect(m_server, &QTcpServer::newConnection, this, &ShowroomProxyServer::onNewConnection);

    qCInfo(lcShowroomProxy) << "Local HLS proxy listening on 127.0.0.1:" << m_localPort;
}

void ShowroomProxyServer::stop()
{
    if (!m_running)
        return;
    m_server->close();
    setRunning(false);
    setLocalPort(0);
    qCInfo(lcShowroomProxy) << "Local HLS proxy stopped";
}

void ShowroomProxyServer::setRunning(bool running)
{
    if (m_running == running)
        return;
    m_running = running;
    emit runningChanged();
}

void ShowroomProxyServer::setLocalPort(quint16 port)
{
    if (m_localPort == port)
        return;
    m_localPort = port;
    emit localPortChanged();
}

void ShowroomProxyServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (!socket)
            continue;
        socket->setParent(this);
        handleConnection(socket);
    }
}

void ShowroomProxyServer::handleConnection(QTcpSocket *socket)
{
    auto *buffer = new QByteArray();

    connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer]() {
        buffer->append(socket->readAll());
        const int headerEnd = buffer->indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;

        const QByteArray rawHeaders = buffer->left(headerEnd);
        buffer->remove(0, headerEnd + 4);

        const int firstLineEnd = rawHeaders.indexOf("\r\n");
        const QByteArray requestLine =
            firstLineEnd < 0 ? rawHeaders : rawHeaders.left(firstLineEnd);
        const QList<QByteArray> parts = requestLine.split(' ');
        if (parts.size() < 2) {
            socket->close();
            return;
        }

        const QUrl realUrl = decodeUrlFromPath(QString::fromLatin1(parts.at(1)));
        if (!realUrl.isValid()) {
            qCWarning(lcShowroomProxy) << "Could not decode proxy URL from path" << parts.at(1);
            socket->close();
            return;
        }

        const QByteArray rangeHeader = extractHeaderValue(rawHeaders, "Range");
        serveUpstream(socket, realUrl, rangeHeader);
    });

    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

QNetworkRequest ShowroomProxyServer::buildUpstreamRequest(const QUrl &url,
                                                          const QByteArray &rangeHeader) const
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    request.setRawHeader("User-Agent", m_userAgent.toUtf8());
    request.setRawHeader("Accept", "*/*");
    request.setRawHeader("Accept-Language",
                         "en-US,en;q=0.9,ja;q=0.8,zh-CN;q=0.7,zh;q=0.6,zh-TW;q=0.5");
    request.setRawHeader("Origin", kOrigin);
    request.setRawHeader("Referer", kReferer);
    request.setRawHeader("DNT", "1");
    request.setRawHeader("sec-ch-ua",
                         "\"Google Chrome\";v=\"149\", \"Chromium\";v=\"149\", \"Not)A;Brand\";v=\"24\"");
    request.setRawHeader("sec-ch-ua-mobile", "?0");
    request.setRawHeader("sec-ch-ua-platform", "\"macOS\"");
    request.setRawHeader("Sec-Fetch-Site", "cross-site");
    request.setRawHeader("Sec-Fetch-Mode", "cors");
    request.setRawHeader("Sec-Fetch-Dest", "empty");
    request.setRawHeader("Priority", "u=1, i");

    if (!rangeHeader.isEmpty())
        request.setRawHeader("Range", rangeHeader);

    return request;
}

void ShowroomProxyServer::serveUpstream(QTcpSocket *socket, const QUrl &realUrl,
                                        const QByteArray &rangeHeader)
{
    qCDebug(lcShowroomProxy) << "Upstream GET" << realUrl.toString(QUrl::RemovePassword);

    QNetworkRequest request = buildUpstreamRequest(realUrl, rangeHeader);
    QNetworkReply *reply = m_nam->get(request);
    reply->setParent(socket);

    connect(reply, &QNetworkReply::finished, this, [this, socket, reply, realUrl]() {
        if (socket->state() == QAbstractSocket::UnconnectedState) {
            reply->deleteLater();
            return;
        }

        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray contentType =
            reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
        const QByteArray statusText = [&]() -> QByteArray {
            switch (statusCode) {
            case 200: return "OK";
            case 206: return "Partial Content";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 304: return "Not Modified";
            case 400: return "Bad Request";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            case 0:   return "Bad Gateway";
            default:  return "OK";
            }
        }();

        if (reply->error() != QNetworkReply::NoError && statusCode == 0) {
            qCWarning(lcShowroomProxy) << "Upstream failed for" << realUrl.host()
                                       << ":" << reply->errorString();
            const QByteArray body = reply->errorString().toUtf8();
            const QByteArray response =
                "HTTP/1.1 502 Bad Gateway\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                "Content-Length: " +
                QByteArray::number(body.size()) +
                "\r\n"
                "Connection: close\r\n"
                "\r\n" +
                body;
            socket->write(response);
            socket->flush();
            socket->close();
            reply->deleteLater();
            return;
        }

        QByteArray body = reply->readAll();

        const bool isPlaylist =
            isPlaylistContentType(contentType) || (contentType.isEmpty() && isPlaylistPath(realUrl));
        if (isPlaylist && statusCode == 200)
            body = rewritePlaylist(body, realUrl);

        QByteArray headerBlock;
        headerBlock.append("HTTP/1.1 ");
        headerBlock.append(QByteArray::number(statusCode > 0 ? statusCode : 200));
        headerBlock.append(" ");
        headerBlock.append(statusText);
        headerBlock.append("\r\n");

        if (!contentType.isEmpty()) {
            headerBlock.append("Content-Type: ");
            headerBlock.append(contentType);
            headerBlock.append("\r\n");
        }
        if (statusCode == 206) {
            const QByteArray range = reply->rawHeader("Content-Range");
            if (!range.isEmpty()) {
                headerBlock.append("Content-Range: ");
                headerBlock.append(range);
                headerBlock.append("\r\n");
            }
        }
        headerBlock.append("Content-Length: ");
        headerBlock.append(QByteArray::number(body.size()));
        headerBlock.append("\r\n");
        headerBlock.append("Cache-Control: no-store\r\n");
        headerBlock.append("Connection: close\r\n");
        headerBlock.append("\r\n");

        socket->write(headerBlock);
        socket->write(body);
        socket->flush();
        socket->close();
        reply->deleteLater();
    });

    connect(socket, &QTcpSocket::disconnected, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
}

QByteArray ShowroomProxyServer::rewritePlaylist(const QByteArray &data, const QUrl &playlistUrl) const
{
    const QString text = QString::fromUtf8(data);
    const QStringList lines = text.split('\n');
    QString rewritten;
    rewritten.reserve(text.size() + 64);

    for (QString line : lines) {
        if (line.endsWith('\r'))
            line.chop(1);

        QString out = line;

        if (!line.isEmpty() && !line.startsWith('#')) {
            out = rewriteUrlString(line.trimmed(), playlistUrl);
        } else if (line.startsWith(QStringLiteral("#EXT-X-KEY")) ||
                   line.startsWith(QStringLiteral("#EXT-X-MAP")) ||
                   line.startsWith(QStringLiteral("#EXT-X-MEDIA"))) {
            static const QRegularExpression uriRegex(
                QStringLiteral("URI=\"([^\"]+)\""));
            QRegularExpressionMatchIterator it = uriRegex.globalMatch(line);
            QString replaced = line;
            int offsetShift = 0;
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                const QString original = match.captured(1);
                const QString rewrote = rewriteUrlString(original, playlistUrl);
                const int start = match.capturedStart(1) + offsetShift;
                const int len = match.capturedLength(1);
                replaced.replace(start, len, rewrote);
                offsetShift += rewrote.length() - len;
            }
            out = replaced;
        }

        rewritten.append(out);
        rewritten.append('\n');
    }

    return rewritten.toUtf8();
}

QString ShowroomProxyServer::rewriteUrlString(const QString &maybeUrl, const QUrl &baseUrl) const
{
    if (maybeUrl.isEmpty())
        return maybeUrl;

    QUrl resolved;
    if (maybeUrl.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
        maybeUrl.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        resolved = QUrl(maybeUrl);
    } else {
        resolved = baseUrl.resolved(QUrl(maybeUrl));
    }

    if (!resolved.isValid())
        return maybeUrl;

    return rewriteUrl(resolved.toString(QUrl::FullyEncoded));
}
