#include "ShowroomProxy.h"
#include "ShowroomLog.h"

#include <QQmlEngine>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QStandardPaths>

ShowroomProxy *ShowroomProxy::s_instance = nullptr;

namespace {

QString proxyFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())
        return {};
    QDir().mkpath(dir);
    return dir + QStringLiteral("/proxy.json");
}

} // namespace

ShowroomProxy::ShowroomProxy(QObject *parent)
    : QObject(parent)
{
    loadFromDisk();
    apply();
}

ShowroomProxy *ShowroomProxy::create(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    if (!s_instance)
        s_instance = new ShowroomProxy();
    QQmlEngine::setObjectOwnership(s_instance, QQmlEngine::CppOwnership);
    return s_instance;
}

ShowroomProxy *ShowroomProxy::instance()
{
    if (!s_instance)
        s_instance = new ShowroomProxy();
    return s_instance;
}

void ShowroomProxy::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
    markDirty();
}

void ShowroomProxy::setType(int type)
{
    const int clamped = (type == static_cast<int>(ProxyType::Socks5))
                            ? static_cast<int>(ProxyType::Socks5)
                            : static_cast<int>(ProxyType::Http);
    if (m_type == clamped)
        return;
    m_type = clamped;
    emit typeChanged();
    markDirty();
}

void ShowroomProxy::setHost(const QString &host)
{
    const QString trimmed = host.trimmed();
    if (m_host == trimmed)
        return;
    m_host = trimmed;
    emit hostChanged();
    markDirty();
}

void ShowroomProxy::setPort(int port)
{
    const int clamped = qBound(0, port, 65535);
    if (m_port == clamped)
        return;
    m_port = clamped;
    emit portChanged();
    markDirty();
}

void ShowroomProxy::setUsername(const QString &username)
{
    if (m_username == username)
        return;
    m_username = username;
    emit usernameChanged();
    markDirty();
}

void ShowroomProxy::setPassword(const QString &password)
{
    if (m_password == password)
        return;
    m_password = password;
    emit passwordChanged();
    markDirty();
}

void ShowroomProxy::load()
{
    if (loadFromDisk())
        setDirty(false);
}

bool ShowroomProxy::save()
{
    const QString path = proxyFilePath();
    if (path.isEmpty()) {
        qCWarning(lcShowroomProxy) << "Cannot resolve proxy config path";
        return false;
    }

    QJsonObject object;
    object[QStringLiteral("enabled")] = m_enabled;
    object[QStringLiteral("type")] = m_type;
    object[QStringLiteral("host")] = m_host;
    object[QStringLiteral("port")] = m_port;
    object[QStringLiteral("username")] = m_username;
    object[QStringLiteral("password")] = m_password;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcShowroomProxy) << "Failed to write proxy config:" << path;
        return false;
    }

    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    qCInfo(lcShowroomProxy) << "Saved proxy config to" << path;
    setDirty(false);
    apply();
    return true;
}

void ShowroomProxy::apply()
{
    QNetworkProxy proxy;

    if (m_enabled && !m_host.isEmpty() && m_port > 0) {
        const QNetworkProxy::ProxyType qtType =
            m_type == static_cast<int>(ProxyType::Socks5) ? QNetworkProxy::Socks5Proxy
                                                          : QNetworkProxy::HttpProxy;
        proxy.setType(qtType);
        proxy.setHostName(m_host);
        proxy.setPort(static_cast<quint16>(m_port));
        if (!m_username.isEmpty())
            proxy.setUser(m_username);
        if (!m_password.isEmpty())
            proxy.setPassword(m_password);
    } else {
        proxy.setType(QNetworkProxy::NoProxy);
    }

    QNetworkProxy::setApplicationProxy(proxy);

    const QString summary = m_enabled && !m_host.isEmpty() && m_port > 0
        ? QStringLiteral("%1://%2:%3")
              .arg(m_type == static_cast<int>(ProxyType::Socks5) ? QStringLiteral("socks5")
                                                                  : QStringLiteral("http"),
                   m_host)
              .arg(m_port)
        : QStringLiteral("disabled");

    qCInfo(lcShowroomProxy) << "Application proxy applied:" << summary;
    emit proxyApplied(summary);
}

void ShowroomProxy::revert()
{
    if (loadFromDisk())
        setDirty(false);
    apply();
}

void ShowroomProxy::clear()
{
    setEnabled(false);
    setType(static_cast<int>(ProxyType::Http));
    setHost({});
    setPort(0);
    setUsername({});
    setPassword({});
}

void ShowroomProxy::markDirty()
{
    setDirty(true);
}

void ShowroomProxy::setDirty(bool dirty)
{
    if (m_dirty == dirty)
        return;
    m_dirty = dirty;
    emit dirtyChanged();
}

bool ShowroomProxy::loadFromDisk()
{
    const QString path = proxyFilePath();
    if (path.isEmpty() || !QFile::exists(path)) {
        qCInfo(lcShowroomProxy) << "No saved proxy config at" << path;
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcShowroomProxy) << "Failed to read proxy config:" << path;
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        qCWarning(lcShowroomProxy) << "Invalid proxy config format:" << path;
        return false;
    }

    const QJsonObject object = document.object();

    m_enabled = object.value(QStringLiteral("enabled")).toBool(false);
    m_type = object.value(QStringLiteral("type")).toInt(static_cast<int>(ProxyType::Http));
    m_host = object.value(QStringLiteral("host")).toString().trimmed();
    m_port = object.value(QStringLiteral("port")).toInt(0);
    m_username = object.value(QStringLiteral("username")).toString();
    m_password = object.value(QStringLiteral("password")).toString();

    emit enabledChanged();
    emit typeChanged();
    emit hostChanged();
    emit portChanged();
    emit usernameChanged();
    emit passwordChanged();

    qCInfo(lcShowroomProxy) << "Loaded proxy config from" << path
                            << "enabled:" << m_enabled << "host:" << m_host << "port:" << m_port;
    return true;
}
