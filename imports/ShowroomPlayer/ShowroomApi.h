#pragma once

#include <QObject>
#include <QUrlQuery>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

class ShowroomApi : public QObject
{
    Q_OBJECT

public:
    explicit ShowroomApi(QObject *parent = nullptr);

    void get(const QString &resource, const QUrlQuery &query,
             const std::function<void(QNetworkReply *)> &callback);

private:
    QNetworkAccessManager *m_nam;
};
