#include "ApplicationLogging.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>
#include <QQmlEngine>

#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <utility>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <Windows.h>
#endif

namespace {

ApplicationLogging *activeLogger = nullptr;
QtMessageHandler previousMessageHandler = nullptr;
std::mutex handlerMutex;
std::condition_variable handlerIdle;
std::size_t activeHandlerCalls = 0;

class HandlerCallGuard final
{
public:
    explicit HandlerCallGuard(const bool active)
        : m_active(active)
    {
    }

    ~HandlerCallGuard()
    {
        if (!m_active)
            return;
        std::lock_guard<std::mutex> lock(handlerMutex);
        if (--activeHandlerCalls == 0)
            handlerIdle.notify_all();
    }

private:
    bool m_active = false;
};

QString environmentString(const char *name)
{
    return QString::fromUtf8(qgetenv(name));
}

std::optional<bool> environmentBool(const char *name)
{
    if (!qEnvironmentVariableIsSet(name))
        return std::nullopt;

    const QString value = environmentString(name).trimmed().toLower();
    if (value == QLatin1String("1") || value == QLatin1String("true")
        || value == QLatin1String("yes") || value == QLatin1String("on"))
        return true;
    if (value == QLatin1String("0") || value == QLatin1String("false")
        || value == QLatin1String("no") || value == QLatin1String("off"))
        return false;
    return std::nullopt;
}

std::optional<ApplicationLogging::Mode> environmentMode(const char *name)
{
    if (!qEnvironmentVariableIsSet(name))
        return std::nullopt;

    const QString value = environmentString(name).trimmed().toLower();
    if (value == QLatin1String("normal"))
        return ApplicationLogging::Normal;
    if (value == QLatin1String("playback")
        || value == QLatin1String("playback-diagnostics"))
        return ApplicationLogging::PlaybackDiagnostics;
    return std::nullopt;
}

spdlog::level::level_enum spdlogLevel(int severity)
{
    switch (severity) {
    case 0: return spdlog::level::debug;
    case 1: return spdlog::level::info;
    case 2: return spdlog::level::warn;
    case 3: return spdlog::level::err;
    default: return spdlog::level::critical;
    }
}

QString qtLevelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return QStringLiteral("DEBUG");
    case QtInfoMsg: return QStringLiteral("INFO");
    case QtWarningMsg: return QStringLiteral("WARN");
    case QtCriticalMsg: return QStringLiteral("ERROR");
    case QtFatalMsg: return QStringLiteral("FATAL");
    }
    return QStringLiteral("UNKNOWN");
}

std::string internalLogLine(const QString &level, const QString &category, const QString &message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const quintptr threadId = reinterpret_cast<quintptr>(QThread::currentThreadId());
    const QString line = QStringLiteral("[%1] [%2] [thread %3] [%4] %5")
                             .arg(timestamp, level)
                             .arg(threadId, 0, 16)
                             .arg(category, message);
    return line.toUtf8().toStdString();
}

} // namespace

ApplicationLogging *ApplicationLogging::s_instance = nullptr;

ApplicationLogging::ApplicationLogging(QObject *parent)
    : ApplicationLogging(Options{}, parent)
{
}

ApplicationLogging::ApplicationLogging(Options options, QObject *parent)
    : QObject(parent)
    , m_options(std::move(options))
{
    if (m_options.settingsPath.isEmpty())
        m_options.settingsPath = defaultSettingsPath();
    if (m_options.logDirectory.isEmpty())
        m_options.logDirectory = defaultLogDirectory();
    m_options.maxFileSize = qMax<quint64>(m_options.maxFileSize, 1024);
    m_options.backupFileCount = qMax(m_options.backupFileCount, 1);
    m_options.queueCapacity = qMax<std::size_t>(m_options.queueCapacity, 1);

    loadSettings();

    const std::optional<bool> startupFileOverride = environmentBool("SHOWROOM_LOG_FILE_ENABLED");
    const std::optional<Mode> startupModeOverride = environmentMode("SHOWROOM_LOG_MODE");
    m_startupCategoryRules = environmentString("SHOWROOM_LOG_RULES");

    m_savedFileEnabled = m_fileEnabled;
    m_savedMode = m_mode;

    m_qtStartupRules = environmentString("QT_LOGGING_RULES");
    qunsetenv("QT_LOGGING_RULES");

    if (startupFileOverride)
        m_fileEnabled = *startupFileOverride;
    if (startupModeOverride)
        m_mode = *startupModeOverride;

    m_baselineFileEnabled = m_fileEnabled;
    m_baselineMode = m_mode;

    applySettings(m_qtStartupRules + QLatin1Char('\n') + m_startupCategoryRules);
    m_fileEnabledAtomic.store(m_fileEnabled);

    m_worker = std::thread(&ApplicationLogging::workerLoop, this);
    installMessageHandler();

    if (m_fileEnabled)
        initializeFileSink();
    else
        setStatus(tr("File logging is off"), true);
}

ApplicationLogging::~ApplicationLogging()
{
    removeMessageHandler();
    m_stopping.store(true);
    m_queueReady.notify_all();
    if (m_worker.joinable())
        m_worker.join();

    std::lock_guard<std::mutex> lock(handlerMutex);
    if (s_instance == this)
        s_instance = nullptr;
}

ApplicationLogging *ApplicationLogging::initialize(QObject *parent)
{
    if (!s_instance)
        s_instance = new ApplicationLogging(parent);
    return s_instance;
}

ApplicationLogging *ApplicationLogging::create(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    ApplicationLogging *logging = initialize(QCoreApplication::instance());
    QQmlEngine::setObjectOwnership(logging, QQmlEngine::CppOwnership);
    return logging;
}

bool ApplicationLogging::available() const
{
    return !m_fileEnabled || m_availableAtomic.load();
}

QString ApplicationLogging::statusMessage() const
{
    return m_statusMessage;
}

void ApplicationLogging::setFileEnabled(const bool enabled)
{
    if (m_fileEnabled == enabled)
        return;
    m_fileEnabled = enabled;
    m_dirty = m_fileEnabled != m_baselineFileEnabled || m_mode != m_baselineMode;
    emit settingsChanged();
}

void ApplicationLogging::setMode(const Mode mode)
{
    const Mode clamped = mode == PlaybackDiagnostics ? PlaybackDiagnostics : Normal;
    if (m_mode == clamped)
        return;
    m_mode = clamped;
    m_dirty = m_fileEnabled != m_baselineFileEnabled || m_mode != m_baselineMode;
    emit settingsChanged();
}

bool ApplicationLogging::save()
{
    const bool fileEnabledChanged = m_fileEnabled != m_baselineFileEnabled;
    const bool modeChanged = m_mode != m_baselineMode;
    const bool persistedFileEnabled = fileEnabledChanged ? m_fileEnabled : m_savedFileEnabled;
    const Mode persistedMode = modeChanged ? m_mode : m_savedMode;

    if (!writeSettings(persistedFileEnabled, persistedMode)) {
        setStatus(tr("Could not save logging settings"), available());
        return false;
    }

    m_savedFileEnabled = persistedFileEnabled;
    m_savedMode = persistedMode;
    m_baselineFileEnabled = m_fileEnabled;
    m_baselineMode = m_mode;
    m_dirty = false;
    m_qtStartupRules.clear();
    applySettings(m_startupCategoryRules);
    m_fileEnabledAtomic.store(m_fileEnabled);
    if (m_fileEnabled) {
        bool hasSink = false;
        {
            std::lock_guard<std::mutex> lock(m_loggerMutex);
            hasSink = static_cast<bool>(m_logger);
        }
        if (!hasSink || !m_availableAtomic.load())
            initializeFileSink();
    } else {
        setStatus(tr("File logging is off"), true);
    }

    emit settingsChanged();
    return true;
}

void ApplicationLogging::revert()
{
    m_fileEnabled = m_baselineFileEnabled;
    m_mode = m_baselineMode;
    m_dirty = false;

    applySettings(m_qtStartupRules + QLatin1Char('\n') + m_startupCategoryRules);
    m_fileEnabledAtomic.store(m_fileEnabled);
    if (m_fileEnabled && !m_availableAtomic.load())
        initializeFileSink();
    else if (!m_fileEnabled)
        setStatus(tr("File logging is off"), true);

    emit settingsChanged();
}

void ApplicationLogging::openLogDirectory()
{
    if (m_options.logDirectory.isEmpty()) {
        setStatus(tr("Could not determine the log directory"), available());
        return;
    }

    QDir directory(m_options.logDirectory);
    if (!directory.exists() && !QDir().mkpath(directory.absolutePath())) {
        setStatus(tr("Could not create the log directory"), available());
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(directory.absolutePath())))
        setStatus(tr("Could not open the log directory"), available());
}

void ApplicationLogging::flush()
{
    flushQueue();
}

void ApplicationLogging::loadSettings()
{
    m_fileEnabled = true;
    m_mode = Normal;
    m_categoryRules.clear();

    QFile file(m_options.settingsPath);
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly)) {
        m_statusMessage = tr("Could not read logging settings; using defaults");
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        m_statusMessage = tr("Logging settings are invalid; using defaults");
        return;
    }

    const QJsonObject object = document.object();
    m_fileEnabled = object.value(QStringLiteral("fileEnabled")).toBool(true);
    const QString modeValue = object.value(QStringLiteral("mode")).toString();
    m_mode = modeValue == QLatin1String("playback") ? PlaybackDiagnostics : Normal;
    m_categoryRules = object.value(QStringLiteral("categoryRules")).toString();
}

bool ApplicationLogging::writeSettings(const bool fileEnabled, const Mode mode) const
{
    if (m_options.settingsPath.isEmpty())
        return false;

    const QFileInfo fileInfo(m_options.settingsPath);
    if (!QDir().mkpath(fileInfo.absolutePath()))
        return false;

    QSaveFile file(m_options.settingsPath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    const QJsonObject object{
        {QStringLiteral("fileEnabled"), fileEnabled},
        {QStringLiteral("mode"), mode == PlaybackDiagnostics ? QStringLiteral("playback")
                                                              : QStringLiteral("normal")},
        {QStringLiteral("categoryRules"), m_categoryRules},
    };
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size())
        return false;
    return file.commit();
}

void ApplicationLogging::applySettings(const QString &startupRules)
{
    QLoggingCategory::setFilterRules(categoryRules(startupRules));
}

QString ApplicationLogging::categoryRules(const QString &startupRules) const
{
    QString rules = QStringLiteral(
        "showroom.*.debug=false\n"
        "showroom.*.info=true\n"
        "showroom.*.warning=true\n"
        "showroom.*.critical=true\n");

    if (m_mode == PlaybackDiagnostics) {
        rules += QStringLiteral("showroom.player.debug=true\nshowroom.proxy.debug=true\n");
    }

    if (!m_categoryRules.trimmed().isEmpty())
        rules += m_categoryRules.trimmed() + QLatin1Char('\n');
    if (!startupRules.trimmed().isEmpty())
        rules += startupRules.trimmed() + QLatin1Char('\n');

    return rules;
}

void ApplicationLogging::initializeFileSink()
{
    if (!m_fileEnabledAtomic.load())
        return;

    if (m_options.logDirectory.isEmpty()) {
        reportFileError(tr("Could not determine the log directory"));
        return;
    }

    const QString directory = QDir(m_options.logDirectory).absolutePath();
    if (!QDir().mkpath(directory)) {
        reportFileError(tr("Could not create log directory: %1").arg(directory));
        return;
    }

    try {
        const QString filename = QDir(directory).filePath(QStringLiteral("ShowroomPlayer.log"));
#ifdef SPDLOG_WCHAR_FILENAMES
        const spdlog::filename_t spdlogFilename = filename.toStdWString();
#else
        const spdlog::filename_t spdlogFilename = filename.toStdString();
#endif
        auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            spdlogFilename, static_cast<std::size_t>(m_options.maxFileSize),
            static_cast<std::size_t>(m_options.backupFileCount), true);
        auto logger = std::make_shared<spdlog::logger>("ShowroomPlayer", sink);
        logger->set_pattern("%v");
        logger->set_level(spdlog::level::trace);
        logger->set_error_handler([this](const std::string &message) {
            reportFileError(QString::fromStdString(message));
        });

        {
            std::lock_guard<std::mutex> lock(m_loggerMutex);
            m_logger = std::move(logger);
        }
        m_availableAtomic.store(true);
        setStatus(tr("Writing logs to %1").arg(filename), true);
    } catch (const std::exception &error) {
        reportFileError(QString::fromUtf8(error.what()));
    }
}

void ApplicationLogging::enqueue(const int severity, const std::string &text)
{
    if (!m_fileEnabledAtomic.load() || !m_availableAtomic.load() || m_stopping.load())
        return;

    std::unique_lock<std::mutex> lock(m_queueMutex);
    if (m_queue.size() >= m_options.queueCapacity) {
        if (severity <= 1) {
            m_droppedMessages.fetch_add(1);
            m_unreportedDroppedMessages.fetch_add(1);
            lock.unlock();
            reportDroppedMessages();
            return;
        }

        const auto lowPriority = std::find_if(m_queue.begin(), m_queue.end(), [](const Entry &entry) {
            return entry.severity <= 1;
        });
        if (lowPriority != m_queue.end()) {
            m_queue.erase(lowPriority);
        } else {
            m_queue.pop_front();
        }
        m_droppedMessages.fetch_add(1);
        m_unreportedDroppedMessages.fetch_add(1);
    }

    m_queue.push_back({severity, text});
    lock.unlock();
    m_queueReady.notify_one();
}

void ApplicationLogging::workerLoop()
{
    for (;;) {
        Entry entry;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            const bool ready = m_queueReady.wait_for(
                lock, std::chrono::seconds(1),
                [this]() { return m_stopping.load() || !m_queue.empty(); });
            if (!ready) {
                lock.unlock();
                std::shared_ptr<spdlog::logger> logger;
                {
                    std::lock_guard<std::mutex> loggerLock(m_loggerMutex);
                    logger = m_logger;
                }
                if (logger && m_availableAtomic.load()) {
                    try {
                        logger->flush();
                    } catch (const std::exception &error) {
                        reportFileError(QString::fromUtf8(error.what()));
                    }
                }
                continue;
            }
            if (m_queue.empty() && m_stopping.load())
                break;

            entry = std::move(m_queue.front());
            m_queue.pop_front();
            m_workerWriting = true;
        }
        try {
            std::shared_ptr<spdlog::logger> logger;
            {
                std::lock_guard<std::mutex> lock(m_loggerMutex);
                logger = m_logger;
            }

            if (logger && m_availableAtomic.load()) {
                logger->log(spdlogLevel(entry.severity), "{}", entry.text);
                if (entry.severity >= 2)
                    logger->flush();

                writeDropSummary(logger);
            }
        } catch (const std::exception &error) {
            reportFileError(QString::fromUtf8(error.what()));
        }

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_workerWriting = false;
            if (m_queue.empty())
                m_queueDrained.notify_all();
        }
    }

    std::shared_ptr<spdlog::logger> logger;
    {
        std::lock_guard<std::mutex> lock(m_loggerMutex);
        logger = m_logger;
    }
    if (logger) {
        writeDropSummary(logger);
        logger->flush();
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_workerWriting = false;
    m_queueDrained.notify_all();
}

void ApplicationLogging::writeDropSummary(const std::shared_ptr<spdlog::logger> &logger)
{
    const qulonglong dropped = m_unreportedDroppedMessages.exchange(0);
    if (dropped == 0)
        return;

    const QString message = QStringLiteral("Logging queue discarded %1 entries").arg(dropped);
    logger->log(spdlog::level::warn, "{}",
                internalLogLine(QStringLiteral("WARN"), QStringLiteral("showroom.logging"),
                                message));
    logger->flush();
}

void ApplicationLogging::flushQueue()
{
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_queueDrained.wait(lock, [this]() { return m_queue.empty() && !m_workerWriting; });
    lock.unlock();

    std::shared_ptr<spdlog::logger> logger;
    {
        std::lock_guard<std::mutex> loggerLock(m_loggerMutex);
        logger = m_logger;
    }
    if (logger) {
        try {
            logger->flush();
        } catch (const std::exception &error) {
            reportFileError(QString::fromUtf8(error.what()));
        }
    }
}

void ApplicationLogging::reportDroppedMessages()
{
    if (m_dropNotificationPending.exchange(true))
        return;

    QMetaObject::invokeMethod(this, [this]() {
        m_dropNotificationPending.store(false);
        emit runtimeStatusChanged();
    }, Qt::QueuedConnection);
}

void ApplicationLogging::reportFileError(const QString &message)
{
    m_availableAtomic.store(false);
    const QString safeMessage = redactSensitiveData(message);
    QtMessageHandler systemHandler = nullptr;
    {
        std::lock_guard<std::mutex> lock(handlerMutex);
        systemHandler = previousMessageHandler;
    }

    const QMessageLogContext context(nullptr, 0, nullptr, "showroom.logging");
    if (systemHandler && systemHandler != &ApplicationLogging::messageHandler) {
        systemHandler(QtWarningMsg, context, safeMessage);
    } else {
        const std::string line = internalLogLine(QStringLiteral("WARN"),
                                                 QStringLiteral("showroom.logging"),
                                                 safeMessage);
        QByteArray output(line.data(), static_cast<qsizetype>(line.size()));
        output.append('\n');
#ifdef Q_OS_WIN
        OutputDebugStringA(output.constData());
#else
        std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stderr);
        std::fflush(stderr);
#endif
    }

    const auto updateStatus = [this, message]() {
        setStatus(tr("File logging unavailable: %1").arg(message), false);
    };
    if (QThread::currentThread() == thread())
        updateStatus();
    else
        QMetaObject::invokeMethod(this, updateStatus, Qt::QueuedConnection);
}

void ApplicationLogging::setStatus(const QString &message, const bool isAvailable)
{
    const bool changed = m_statusMessage != message || m_availableAtomic.load() != isAvailable;
    m_statusMessage = message;
    m_availableAtomic.store(isAvailable);
    if (changed)
        emit runtimeStatusChanged();
}

void ApplicationLogging::installMessageHandler()
{
    std::lock_guard<std::mutex> lock(handlerMutex);
    activeLogger = this;
    previousMessageHandler = qInstallMessageHandler(&ApplicationLogging::messageHandler);
}

void ApplicationLogging::removeMessageHandler()
{
    std::unique_lock<std::mutex> lock(handlerMutex);
    if (activeLogger != this)
        return;

    activeLogger = nullptr;
    qInstallMessageHandler(previousMessageHandler);
    previousMessageHandler = nullptr;
    handlerIdle.wait(lock, []() { return activeHandlerCalls == 0; });
}

QString ApplicationLogging::redactSensitiveData(const QString &message)
{
    QString redacted = message;

    static const QRegularExpression urlUserInfoPattern(
        QStringLiteral("(?i)(https?://)[^/@\\s]+@"));
    redacted.replace(urlUserInfoPattern, QStringLiteral("\\1<REDACTED>@"));

    static const QRegularExpression headerPattern(
        QStringLiteral("(?i)\\b(cookie|set-cookie|authorization|proxy-authorization)\\s*:\\s*[^\\r\\n]*"));
    redacted.replace(headerPattern, QStringLiteral("\\1: <REDACTED>"));

    static const QRegularExpression cookieAssignmentPattern(
        QStringLiteral("(?i)(\\b(?:cookie|set-cookie)\\s*=\\s*)[^\\r\\n]*"));
    redacted.replace(cookieAssignmentPattern, QStringLiteral("\\1<REDACTED>"));

    static const QRegularExpression bearerAssignmentPattern(
        QStringLiteral("(?i)(\\b(?:proxy-)?authorization\\s*[=:]\\s*bearer\\s+)[^\\s,;&]+"));
    redacted.replace(bearerAssignmentPattern, QStringLiteral("\\1<REDACTED>"));

    static const QRegularExpression sensitiveParameterPattern(
        QStringLiteral("(?i)([?&](?:access[_-]?token|refresh[_-]?token|auth(?:entication)?[_-]?token|token|auth|authorization|api[_-]?key|access[_-]?key|key|signature|sig|session(?:[_-]?id)?|sr_id|bcsvr_key|credential|password)=)[^&#\\s]*"));
    redacted.replace(sensitiveParameterPattern, QStringLiteral("\\1<REDACTED>"));

    static const QRegularExpression assignmentPattern(
        QStringLiteral("(?i)([\\\"']?\\b(?:access[_-]?token|refresh[_-]?token|auth(?:entication)?[_-]?token|client[_-]?secret|secret|authorization|proxy[_-]?authorization|bcsvr_key|sr_id|password|cookie|set-cookie|api[_-]?key|access[_-]?key|key|session(?:[_-]?id)?|credential|csrf[_-]?token|xsrf[_-]?token)[\\\"']?\\s*[=:]\\s*[\\\"']?)[^\\\"'\\s,}&]+"));
    redacted.replace(assignmentPattern, QStringLiteral("\\1<REDACTED>"));
    return redacted;
}

QString ApplicationLogging::defaultSettingsPath()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return directory.isEmpty() ? QString() : QDir(directory).filePath(QStringLiteral("logging.json"));
}

QString ApplicationLogging::defaultLogDirectory()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return directory.isEmpty() ? QString() : QDir(directory).filePath(QStringLiteral("logs"));
}

int ApplicationLogging::severityForType(const QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return 0;
    case QtInfoMsg: return 1;
    case QtWarningMsg: return 2;
    case QtCriticalMsg: return 3;
    case QtFatalMsg: return 4;
    }
    return 3;
}

void ApplicationLogging::messageHandler(const QtMsgType type,
                                        const QMessageLogContext &context,
                                        const QString &message)
{
    try {
        const QString safeMessage = redactSensitiveData(message);
        const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        const quintptr threadId = reinterpret_cast<quintptr>(QThread::currentThreadId());
        const QString category = context.category ? QString::fromUtf8(context.category)
                                                   : QStringLiteral("default");
        const QString line = QStringLiteral("[%1] [%2] [thread %3] [%4] %5")
                                 .arg(timestamp, qtLevelName(type))
                                 .arg(threadId, 0, 16)
                                 .arg(category, safeMessage);
        const QByteArray encoded = line.toUtf8();

        QtMessageHandler systemHandler = nullptr;
        ApplicationLogging *logging = nullptr;
        {
            std::lock_guard<std::mutex> lock(handlerMutex);
            logging = activeLogger;
            if (logging) {
                ++activeHandlerCalls;
                systemHandler = previousMessageHandler;
            }
        }
        HandlerCallGuard guard(logging != nullptr);

        if (logging) {
            logging->enqueue(severityForType(type), encoded.toStdString());
            if (type == QtFatalMsg)
                logging->flushQueue();
        }

        if (systemHandler && systemHandler != &ApplicationLogging::messageHandler) {
            systemHandler(type, context, safeMessage);
        } else {
            const QByteArray output = (line + QLatin1Char('\n')).toLocal8Bit();
#ifdef Q_OS_WIN
            OutputDebugStringA(output.constData());
#else
            std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stderr);
            std::fflush(stderr);
#endif
        }
    } catch (...) {
#ifdef Q_OS_WIN
        OutputDebugStringA("ShowroomPlayer: failed to process a Qt log message\n");
#else
        std::fputs("ShowroomPlayer: failed to process a Qt log message\n", stderr);
        std::fflush(stderr);
#endif
    }
}
