#include "ShowroomAuth.h"
#include "ShowroomApi.h"
#include "ShowroomLog.h"
#include "ShowroomSessionStore.h"

#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QTimer>

ShowroomAuth::ShowroomAuth(QObject *parent)
    : QObject(parent)
    , m_api(ShowroomApi::shared(this))
{
    qCInfo(lcShowroomAuth) << "Auth service initialized";
}

ShowroomAuth *ShowroomAuth::create(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    static ShowroomAuth instance;
    QQmlEngine::setObjectOwnership(&instance, QQmlEngine::CppOwnership);
    qCInfo(lcShowroomAuth) << "Auth singleton ready, scheduling session restore";
    QTimer::singleShot(0, &instance, &ShowroomAuth::restoreSession);
    return &instance;
}

void ShowroomAuth::finishSessionCheck()
{
    if (m_sessionCheckDone)
        return;

    m_sessionCheckDone = true;
    emit sessionCheckFinished();
}

void ShowroomAuth::setBusy(bool busy)
{
    if (m_busy == busy)
        return;

    m_busy = busy;
    emit busyChanged();
}

void ShowroomAuth::setLoggedIn(bool loggedIn, const QString &accountId, const QString &userId,
                               const QString &displayName)
{
    const bool accountChanged = m_accountId != accountId;
    const bool userChanged = m_userId != userId;
    const bool nameChanged = m_displayName != displayName;
    const bool loginChanged = m_loggedIn != loggedIn;

    m_loggedIn = loggedIn;
    m_accountId = accountId;
    m_userId = userId;
    m_displayName = displayName;

    if (loginChanged)
        emit loggedInChanged();
    if (accountChanged)
        emit accountIdChanged();
    if (userChanged)
        emit userIdChanged();
    if (nameChanged)
        emit displayNameChanged();
}

void ShowroomAuth::applyAccountResponse(const QJsonObject &root)
{
    const QString account = root.value(QLatin1String("account_id")).toString();
    const QString userId = QString::number(root.value(QLatin1String("user_id")).toVariant().toLongLong());
    const QString name = root.value(QLatin1String("name")).toString();

    setLoggedIn(true, account, userId, name);
    persistSession();
}

void ShowroomAuth::persistSession()
{
    const QString srId = m_api->srIdCookie();
    if (srId.isEmpty()) {
        qCWarning(lcShowroomAuth) << "Skip session save: sr_id cookie missing";
        return;
    }

    ShowroomSessionStore::saveSrId(srId);
}

void ShowroomAuth::clearSession()
{
    ShowroomSessionStore::clear();
}

QString ShowroomAuth::parseCsrfToken(const QByteArray &html)
{
    static const QRegularExpression pattern(
        QStringLiteral("name=\"csrf_token\"\\s+value=\"([^\"]+)\""));
    const QRegularExpressionMatch match = pattern.match(QString::fromUtf8(html));
    if (match.hasMatch())
        return match.captured(1);

    static const QRegularExpression fallback(
        QStringLiteral("name='csrf_token'\\s+value='([^']+)'"));
    const QRegularExpressionMatch fallbackMatch = fallback.match(QString::fromUtf8(html));
    if (fallbackMatch.hasMatch())
        return fallbackMatch.captured(1);

    return {};
}

void ShowroomAuth::restoreSession()
{
    qCInfo(lcShowroomAuth) << "restoreSession invoked";

    if (m_loggedIn) {
        finishSessionCheck();
        return;
    }

    if (m_busy)
        return;

    const QString savedSrId = ShowroomSessionStore::loadSrId();
    if (savedSrId.isEmpty()) {
        qCInfo(lcShowroomAuth) << "No saved session to restore";
        finishSessionCheck();
        return;
    }

    qCInfo(lcShowroomAuth) << "Restoring session from saved sr_id";
    setBusy(true);
    m_api->clearCookies();
    m_api->setSrIdCookie(savedSrId);

    m_api->getAccount([this](QNetworkReply *reply) {
        setBusy(false);

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError && statusCode == 0) {
            qCWarning(lcShowroomAuth) << "Session restore failed:" << reply->errorString();
            clearSession();
            m_api->clearCookies();
            setLoggedIn(false);
            finishSessionCheck();
            return;
        }

        if (statusCode == 404 || statusCode == 401 || statusCode == 403) {
            qCWarning(lcShowroomAuth) << "Saved session invalid, status:" << statusCode;
            clearSession();
            m_api->clearCookies();
            setLoggedIn(false);
            finishSessionCheck();
            return;
        }

        if (statusCode != 200) {
            qCWarning(lcShowroomAuth) << "Session restore unexpected status:" << statusCode;
            setLoggedIn(false);
            finishSessionCheck();
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(body);
        if (!document.isObject()) {
            qCWarning(lcShowroomAuth) << "Session restore invalid JSON response";
            clearSession();
            m_api->clearCookies();
            setLoggedIn(false);
            finishSessionCheck();
            return;
        }

        const QJsonObject root = document.object();
        if (root.contains(QLatin1String("errors"))) {
            qCWarning(lcShowroomAuth) << "Session restore rejected by API";
            clearSession();
            m_api->clearCookies();
            setLoggedIn(false);
            finishSessionCheck();
            return;
        }

        applyAccountResponse(root);
        qCInfo(lcShowroomAuth) << "Session restored for" << m_accountId;
        emit sessionRestored(m_accountId);
        finishSessionCheck();
    });
}

void ShowroomAuth::login(const QString &accountId, const QString &password)
{
    const QString trimmedAccount = accountId.trimmed();
    if (trimmedAccount.isEmpty()) {
        emit loginFailed(tr("Account ID cannot be empty"));
        return;
    }

    if (password.isEmpty()) {
        emit loginFailed(tr("Password cannot be empty"));
        return;
    }

    if (m_busy) {
        qCDebug(lcShowroomAuth) << "Login ignored: request already in progress";
        return;
    }

    qCInfo(lcShowroomAuth) << "Login requested for account:" << trimmedAccount;
    setBusy(true);
    fetchCsrfAndLogin(trimmedAccount, password);
}

void ShowroomAuth::fetchCsrfAndLogin(const QString &accountId, const QString &password)
{
    m_api->getDocument(QStringLiteral("/"), [this, accountId, password](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            setBusy(false);
            const QString message = tr("Failed to fetch login page: %1").arg(reply->errorString());
            qCCritical(lcShowroomAuth) << message;
            emit loginFailed(message);
            return;
        }

        const QByteArray html = reply->readAll();
        const QString csrfToken = parseCsrfToken(html);
        if (csrfToken.isEmpty()) {
            setBusy(false);
            const QString message = tr("CSRF token not found on login page");
            qCCritical(lcShowroomAuth) << message;
            emit loginFailed(message);
            return;
        }

        qCDebug(lcShowroomAuth) << "Visitor cookies acquired, sr_id present:"
                                << !m_api->srIdCookie().isEmpty();

        const QHash<QString, QString> fields = {
            {QStringLiteral("csrf_token"), csrfToken},
            {QStringLiteral("account_id"), accountId},
            {QStringLiteral("password"), password},
            {QStringLiteral("captcha_word"), QString()},
        };

        m_api->postMultipart(QStringLiteral("user/login"), fields,
                             [this, accountId](QNetworkReply *loginReply) {
                                 if (loginReply->error() != QNetworkReply::NoError) {
                                     setBusy(false);
                                     const QString message =
                                         tr("Login request failed: %1").arg(loginReply->errorString());
                                     qCCritical(lcShowroomAuth) << message;
                                     emit loginFailed(message);
                                     return;
                                 }

                                 const QJsonDocument loginDocument =
                                     QJsonDocument::fromJson(loginReply->readAll());
                                 if (!loginDocument.isObject()) {
                                     setBusy(false);
                                     const QString message = tr("Invalid login response");
                                     qCCritical(lcShowroomAuth) << message;
                                     emit loginFailed(message);
                                     return;
                                 }

                                 const QJsonObject loginRoot = loginDocument.object();
                                 const int ok = loginRoot.value(QLatin1String("ok")).toInt(0);
                                 if (ok != 1) {
                                     setBusy(false);
                                     const QString message =
                                         loginRoot.value(QLatin1String("error")).toString(tr("Login failed"));
                                     qCWarning(lcShowroomAuth) << "Login rejected:" << message;
                                     emit loginFailed(message);
                                     return;
                                 }

                                 const QString loginUserId =
                                     loginRoot.value(QLatin1String("user_id")).toVariant().toString();

                                 m_api->getAccount([this, accountId, loginUserId](QNetworkReply *accountReply) {
                                     setBusy(false);

                                     const int statusCode =
                                         accountReply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                                             .toInt();
                                     const QJsonDocument accountDocument =
                                         QJsonDocument::fromJson(accountReply->readAll());

                                     if (statusCode == 200 && accountDocument.isObject()
                                         && !accountDocument.object().contains(QLatin1String("errors"))) {
                                         applyAccountResponse(accountDocument.object());
                                     } else {
                                         setLoggedIn(true, accountId, loginUserId);
                                         persistSession();
                                     }

                                     qCInfo(lcShowroomAuth) << "Login succeeded, account:"
                                                            << m_accountId
                                                            << "sr_id saved:"
                                                            << !m_api->srIdCookie().isEmpty();
                                     emit loginSucceeded(m_accountId);
                                 });
                             });
    });
}

void ShowroomAuth::logout()
{
    if (m_busy)
        return;

    qCInfo(lcShowroomAuth) << "Logout requested";
    m_api->clearCookies();
    clearSession();
    setLoggedIn(false);
    m_sessionCheckDone = false;
}
