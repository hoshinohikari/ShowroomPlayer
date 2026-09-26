#pragma once

#include <QObject>
#include <QString>
#include <QtLogging>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

class QQmlEngine;
class QJSEngine;
namespace spdlog { class logger; }

class ApplicationLogging : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ShowroomLogging)
    QML_SINGLETON
    Q_PROPERTY(bool fileEnabled READ fileEnabled WRITE setFileEnabled NOTIFY settingsChanged)
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY settingsChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY settingsChanged)
    Q_PROPERTY(bool available READ available NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString logDirectory READ logDirectory CONSTANT)
    Q_PROPERTY(qulonglong droppedMessages READ droppedMessages NOTIFY runtimeStatusChanged)

public:
    enum Mode {
        Normal = 0,
        PlaybackDiagnostics = 1,
    };
    Q_ENUM(Mode)

    struct Options {
        QString settingsPath;
        QString logDirectory;
        quint64 maxFileSize = 10 * 1024 * 1024;
        int backupFileCount = 3;
        std::size_t queueCapacity = 8192;
    };

    explicit ApplicationLogging(QObject *parent = nullptr);
    explicit ApplicationLogging(Options options, QObject *parent = nullptr);
    ~ApplicationLogging() override;

    static ApplicationLogging *initialize(QObject *parent = nullptr);
    static ApplicationLogging *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    bool fileEnabled() const { return m_fileEnabled; }
    Mode mode() const { return m_mode; }
    bool dirty() const { return m_dirty; }
    bool available() const;
    QString statusMessage() const;
    QString logDirectory() const { return m_options.logDirectory; }
    qulonglong droppedMessages() const { return m_droppedMessages.load(); }

    void setFileEnabled(bool enabled);
    void setMode(Mode mode);

    Q_INVOKABLE bool save();
    Q_INVOKABLE void revert();
    Q_INVOKABLE void openLogDirectory();
    void flush();

signals:
    void settingsChanged();
    void runtimeStatusChanged();

private:
    struct Entry {
        int severity = 0;
        std::string text;
    };

    void loadSettings();
    bool writeSettings(bool fileEnabled, Mode mode) const;
    void applySettings(const QString &startupRules = {});
    QString categoryRules(const QString &startupRules = {}) const;
    void initializeFileSink();
    void enqueue(int severity, const std::string &text);
    void workerLoop();
    void flushQueue();
    void writeDropSummary(const std::shared_ptr<spdlog::logger> &logger);
    void reportDroppedMessages();
    void reportFileError(const QString &message);
    void setStatus(const QString &message, bool available);
    void installMessageHandler();
    void removeMessageHandler();

    static QString redactSensitiveData(const QString &message);
    static QString defaultSettingsPath();
    static QString defaultLogDirectory();
    static int severityForType(QtMsgType type);
    static void messageHandler(QtMsgType type, const QMessageLogContext &context,
                               const QString &message);

    Options m_options;
    bool m_fileEnabled = true;
    Mode m_mode = Normal;
    QString m_categoryRules;
    bool m_dirty = false;
    QString m_statusMessage;
    bool m_savedFileEnabled = true;
    Mode m_savedMode = Normal;
    bool m_baselineFileEnabled = true;
    Mode m_baselineMode = Normal;
    QString m_startupCategoryRules;
    QString m_qtStartupRules;
    std::atomic_bool m_fileEnabledAtomic{true};
    std::atomic_bool m_availableAtomic{false};
    std::atomic<qulonglong> m_droppedMessages{0};
    std::atomic<qulonglong> m_unreportedDroppedMessages{0};
    std::atomic_bool m_dropNotificationPending{false};
    std::atomic_bool m_stopping{false};

    mutable std::mutex m_loggerMutex;
    std::shared_ptr<spdlog::logger> m_logger;

    std::mutex m_queueMutex;
    std::condition_variable m_queueReady;
    std::condition_variable m_queueDrained;
    std::deque<Entry> m_queue;
    bool m_workerWriting = false;
    std::thread m_worker;

    static ApplicationLogging *s_instance;
};
