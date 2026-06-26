#pragma once

#include <QHash>
#include <QNetworkRequest>
#include <QObject>
#include <QUrlQuery>

#include <functional>

class QNetworkAccessManager;
class QNetworkCookieJar;
class QNetworkReply;

class ShowroomApi : public QObject
{
    Q_OBJECT

public:
    explicit ShowroomApi(QObject *parent = nullptr);

    static ShowroomApi *shared(QObject *parent = nullptr);

    void get(const QString &resource, const QUrlQuery &query,
             const std::function<void(QNetworkReply *)> &callback);

    void getDocument(const QString &resource,
                     const std::function<void(QNetworkReply *)> &callback);

    void postMultipart(const QString &resource, const QHash<QString, QString> &fields,
                       const std::function<void(QNetworkReply *)> &callback);

    void clearCookies();
    QString srIdCookie() const;
    void setSrIdCookie(const QString &srId);

    void getAccount(const std::function<void(QNetworkReply *)> &callback);

private:
    QNetworkRequest makeRequest(const QUrl &url, bool documentRequest = false) const;
    void finishRequest(QNetworkReply *reply, const QString &resource,
                       const std::function<void(QNetworkReply *)> &callback);

    QNetworkAccessManager *m_nam;
    QNetworkCookieJar *m_cookieJar;
};
