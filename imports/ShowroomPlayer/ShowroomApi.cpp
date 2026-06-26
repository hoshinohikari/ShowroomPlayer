#include "ShowroomApi.h"
#include "ShowroomLog.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

constexpr auto kBaseUrl = "https://www.showroom-live.com/";

QNetworkRequest makeRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                      "(KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36");
    return request;
}

} // namespace

ShowroomApi::ShowroomApi(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    qCInfo(lcShowroomApi) << "HTTP client ready, base URL:" << kBaseUrl;
}

void ShowroomApi::get(const QString &resource, const QUrlQuery &query,
                      const std::function<void(QNetworkReply *)> &callback)
{
    QUrl url(QString::fromLatin1(kBaseUrl) + resource);
    url.setQuery(query);

    qCDebug(lcShowroomApi) << "GET" << url.toString(QUrl::RemovePassword);

    QNetworkReply *reply = m_nam->get(makeRequest(url));
    connect(reply, &QNetworkReply::finished, this, [reply, resource, callback]() {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcShowroomApi) << "GET" << resource
                                     << "failed:" << reply->errorString()
                                     << "status:" << statusCode;
        } else {
            qCDebug(lcShowroomApi) << "GET" << resource << "status:" << statusCode;
        }

        callback(reply);
        reply->deleteLater();
    });
}
