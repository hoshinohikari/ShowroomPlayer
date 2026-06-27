#include "ShowroomApi.h"
#include "ShowroomLog.h"

#include <QDateTime>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

constexpr auto kBaseUrl = "https://www.showroom-live.com/";
constexpr auto kUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36";

class ClearableCookieJar : public QNetworkCookieJar
{
public:
    explicit ClearableCookieJar(QObject *parent = nullptr)
        : QNetworkCookieJar(parent)
    {
    }

    void clearAll()
    {
        setAllCookies({});
    }

    void removeNamed(const QByteArray &name)
    {
        QList<QNetworkCookie> kept;
        const QList<QNetworkCookie> cookies = allCookies();
        kept.reserve(cookies.size());
        for (const QNetworkCookie &cookie : cookies) {
            if (cookie.name() != name)
                kept.append(cookie);
        }
        setAllCookies(kept);
    }
};

QUrl buildUrl(const QString &resource)
{
    QString path = resource;
    while (path.startsWith(QLatin1Char('/')))
        path.remove(0, 1);
    return QUrl(QString::fromLatin1(kBaseUrl) + path);
}

} // namespace

ShowroomApi::ShowroomApi(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_cookieJar(new ClearableCookieJar())
{
    m_nam->setCookieJar(m_cookieJar);
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    qCInfo(lcShowroomApi) << "HTTP client ready, base URL:" << kBaseUrl;
}

ShowroomApi *ShowroomApi::shared(QObject *parent)
{
    static ShowroomApi *instance = nullptr;
    if (!instance)
        instance = new ShowroomApi(parent);
    return instance;
}

QNetworkRequest ShowroomApi::makeRequest(const QUrl &url, bool documentRequest) const
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    if (documentRequest) {
        request.setRawHeader(
            "Accept",
            "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    } else {
        request.setRawHeader("Accept", "application/json, text/javascript, */*; q=0.01");
        request.setRawHeader("X-Requested-With", "XMLHttpRequest");
        request.setRawHeader("Origin", "https://www.showroom-live.com");
        request.setRawHeader("Referer", "https://www.showroom-live.com/");
    }
    return request;
}

void ShowroomApi::finishRequest(QNetworkReply *reply, const QString &resource,
                                const std::function<void(QNetworkReply *)> &callback)
{
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(lcShowroomApi) << resource << "failed:" << reply->errorString()
                               << "status:" << statusCode;
    } else {
        qCDebug(lcShowroomApi) << resource << "status:" << statusCode;
    }

    callback(reply);
    reply->deleteLater();
}

void ShowroomApi::get(const QString &resource, const QUrlQuery &query,
                      const std::function<void(QNetworkReply *)> &callback)
{
    QUrl url = buildUrl(resource);
    url.setQuery(query);

    qCDebug(lcShowroomApi) << "GET" << url.toString(QUrl::RemovePassword);

    QNetworkReply *reply = m_nam->get(makeRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, resource, callback]() {
        finishRequest(reply, QStringLiteral("GET %1").arg(resource), callback);
    });
}

void ShowroomApi::getDocument(const QString &resource,
                              const std::function<void(QNetworkReply *)> &callback)
{
    const QUrl url = buildUrl(resource);

    qCDebug(lcShowroomApi) << "GET document" << url.toString(QUrl::RemovePassword);

    QNetworkReply *reply = m_nam->get(makeRequest(url, true));
    connect(reply, &QNetworkReply::finished, this, [this, reply, resource, callback]() {
        finishRequest(reply, QStringLiteral("GET document %1").arg(resource), callback);
    });
}

void ShowroomApi::postMultipart(const QString &resource, const QHash<QString, QString> &fields,
                                const std::function<void(QNetworkReply *)> &callback)
{
    const QUrl url = buildUrl(resource);
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        QHttpPart part;
        part.setRawHeader(
            "Content-Disposition",
            QByteArray("form-data; name=\"") + it.key().toUtf8() + QByteArray("\""));
        part.setBody(it.value().toUtf8());
        multiPart->append(part);
    }

    qCDebug(lcShowroomApi) << "POST multipart" << url.toString(QUrl::RemovePassword);

    QNetworkRequest request = makeRequest(url);
    QNetworkReply *reply = m_nam->post(request, multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, resource, callback]() {
        finishRequest(reply, QStringLiteral("POST %1").arg(resource), callback);
    });
}

void ShowroomApi::clearCookies()
{
    static_cast<ClearableCookieJar *>(m_cookieJar)->clearAll();
    qCInfo(lcShowroomApi) << "Cookies cleared";
}

QString ShowroomApi::srIdCookie() const
{
    const QList<QNetworkCookie> cookies = m_cookieJar->cookiesForUrl(QUrl(QLatin1String(kBaseUrl)));
    for (const QNetworkCookie &cookie : cookies) {
        if (cookie.name() == "sr_id")
            return QString::fromUtf8(cookie.value());
    }
    return {};
}

void ShowroomApi::setSrIdCookie(const QString &srId)
{
    const QString trimmed = srId.trimmed();
    if (trimmed.isEmpty())
        return;

    auto *jar = static_cast<ClearableCookieJar *>(m_cookieJar);
    jar->removeNamed(QByteArrayLiteral("sr_id"));

    QNetworkCookie cookie(QByteArrayLiteral("sr_id"), trimmed.toUtf8());
    cookie.setDomain(QStringLiteral(".showroom-live.com"));
    cookie.setPath(QStringLiteral("/"));
    cookie.setSecure(true);
    cookie.setExpirationDate(QDateTime::currentDateTimeUtc().addYears(1));
    jar->insertCookie(cookie);
    qCDebug(lcShowroomApi) << "sr_id cookie injected";
}

QList<QNetworkCookie> ShowroomApi::cookiesForUrl(const QUrl &url) const
{
    return m_cookieJar->cookiesForUrl(url);
}

void ShowroomApi::getAccount(const std::function<void(QNetworkReply *)> &callback)
{
    const QUrl url = buildUrl(QStringLiteral("api/account/"));

    qCDebug(lcShowroomApi) << "GET account" << url.toString(QUrl::RemovePassword);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("Referer", "https://www.showroom-live.com/");

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        finishRequest(reply, QStringLiteral("GET api/account/"), callback);
    });
}
