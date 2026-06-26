#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class ShowroomApi;

class ShowroomAuth : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString accountId READ accountId NOTIFY accountIdChanged)
    Q_PROPERTY(QString userId READ userId NOTIFY userIdChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayNameChanged)

public:
    explicit ShowroomAuth(QObject *parent = nullptr);

    static ShowroomAuth *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    bool loggedIn() const { return m_loggedIn; }
    bool busy() const { return m_busy; }
    bool sessionCheckDone() const { return m_sessionCheckDone; }
    QString accountId() const { return m_accountId; }
    QString userId() const { return m_userId; }
    QString displayName() const { return m_displayName; }

public slots:
    void login(const QString &accountId, const QString &password);
    void logout();
    void restoreSession();

signals:
    void loggedInChanged();
    void busyChanged();
    void accountIdChanged();
    void userIdChanged();
    void displayNameChanged();
    void loginSucceeded(const QString &accountId);
    void loginFailed(const QString &message);
    void sessionRestored(const QString &accountId);
    void sessionCheckFinished();

private:
    void setBusy(bool busy);
    void setLoggedIn(bool loggedIn, const QString &accountId = {}, const QString &userId = {},
                     const QString &displayName = {});
    void applyAccountResponse(const QJsonObject &root);
    void persistSession();
    void clearSession();
    void finishSessionCheck();
    void fetchCsrfAndLogin(const QString &accountId, const QString &password);
    static QString parseCsrfToken(const QByteArray &html);

    ShowroomApi *m_api;
    bool m_loggedIn = false;
    bool m_busy = false;
    bool m_sessionCheckDone = false;
    QString m_accountId;
    QString m_userId;
    QString m_displayName;
};
