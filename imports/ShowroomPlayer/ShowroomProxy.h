#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class ShowroomProxy : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)

public:
    enum class ProxyType : int {
        Http = 0,
        Socks5 = 1,
    };
    Q_ENUM(ProxyType)

    explicit ShowroomProxy(QObject *parent = nullptr);

    static ShowroomProxy *create(QQmlEngine *engine, QJSEngine *scriptEngine);
    static ShowroomProxy *instance();

    bool enabled() const { return m_enabled; }
    int type() const { return m_type; }
    QString host() const { return m_host; }
    int port() const { return m_port; }
    QString username() const { return m_username; }
    QString password() const { return m_password; }
    bool dirty() const { return m_dirty; }

    void setEnabled(bool enabled);
    void setType(int type);
    void setHost(const QString &host);
    void setPort(int port);
    void setUsername(const QString &username);
    void setPassword(const QString &password);

    Q_INVOKABLE void load();
    Q_INVOKABLE bool save();
    Q_INVOKABLE void apply();
    Q_INVOKABLE void revert();
    Q_INVOKABLE void clear();

signals:
    void enabledChanged();
    void typeChanged();
    void hostChanged();
    void portChanged();
    void usernameChanged();
    void passwordChanged();
    void dirtyChanged();
    void proxyApplied(const QString &summary);

private:
    void markDirty();
    void setDirty(bool dirty);
    bool loadFromDisk();

    static ShowroomProxy *s_instance;

    bool m_enabled = false;
    int m_type = static_cast<int>(ProxyType::Http);
    QString m_host;
    int m_port = 0;
    QString m_username;
    QString m_password;
    bool m_dirty = false;
};
