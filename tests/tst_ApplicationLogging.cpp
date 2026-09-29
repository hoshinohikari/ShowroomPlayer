#include "ApplicationLogging.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QTemporaryDir>
#include <QtTest>

#include <thread>
#include <vector>

Q_LOGGING_CATEGORY(lcTestPlayer, "showroom.player")
Q_LOGGING_CATEGORY(lcTestProxy, "showroom.proxy")
Q_LOGGING_CATEGORY(lcTestLive, "showroom.live")
Q_LOGGING_CATEGORY(lcTestApi, "showroom.api")
Q_LOGGING_CATEGORY(lcTestController, "showroom.controller")
Q_LOGGING_CATEGORY(lcTestRanking, "showroom.ranking")

namespace {

QMutex capturedMessagesMutex;
QStringList capturedMessages;

void captureSystemMessage(QtMsgType, const QMessageLogContext &, const QString &message)
{
    QMutexLocker lock(&capturedMessagesMutex);
    capturedMessages.append(message);
}

class ScopedMessageHandler final
{
public:
    explicit ScopedMessageHandler(QtMessageHandler handler)
        : m_previous(qInstallMessageHandler(handler))
    {
    }

    ~ScopedMessageHandler()
    {
        qInstallMessageHandler(m_previous);
    }

private:
    QtMessageHandler m_previous = nullptr;
};

class ScopedLoggingEnvironment final
{
public:
    ScopedLoggingEnvironment()
    {
        save("SHOWROOM_LOG_FILE_ENABLED");
        save("SHOWROOM_LOG_MODE");
        save("SHOWROOM_LOG_RULES");
        save("QT_LOGGING_RULES");
        qunsetenv("SHOWROOM_LOG_FILE_ENABLED");
        qunsetenv("SHOWROOM_LOG_MODE");
        qunsetenv("SHOWROOM_LOG_RULES");
        qunsetenv("QT_LOGGING_RULES");
    }

    ~ScopedLoggingEnvironment()
    {
        for (const auto &entry : m_values) {
            if (entry.second.first)
                qputenv(entry.first.constData(), entry.second.second);
            else
                qunsetenv(entry.first.constData());
        }
    }

private:
    void save(const char *name)
    {
        const QByteArray key(name);
        const bool exists = qEnvironmentVariableIsSet(name);
        m_values.emplace_back(key, qMakePair(exists, qgetenv(name)));
    }

    QList<QPair<QByteArray, QPair<bool, QByteArray>>> m_values;
};

ApplicationLogging::Options optionsFor(const QString &root,
                                       const quint64 maxFileSize = 10 * 1024 * 1024,
                                       const std::size_t queueCapacity = 8192)
{
    ApplicationLogging::Options options;
    options.settingsPath = root + QStringLiteral("/config/logging.json");
    options.logDirectory = root + QStringLiteral("/logs");
    options.maxFileSize = maxFileSize;
    options.backupFileCount = 3;
    options.queueCapacity = queueCapacity;
    return options;
}

QString readAllLogs(const QString &directory)
{
    QDir dir(directory);
    const QStringList files = dir.entryList({QStringLiteral("ShowroomPlayer*")},
                                            QDir::Files, QDir::Name);
    QString result;
    for (const QString &name : files) {
        QFile file(dir.filePath(name));
        if (file.open(QIODevice::ReadOnly))
            result += QString::fromUtf8(file.readAll());
    }
    return result;
}

} // namespace

class ApplicationLoggingTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setApplicationName(QStringLiteral("ShowroomPlayerLoggingTest"));
    }

    void defaultInfoAndCategoryFields()
    {
        ScopedLoggingEnvironment environment;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        ApplicationLogging logging(optionsFor(directory.path()));
        QVERIFY(logging.fileEnabled());
        QCOMPARE(logging.mode(), ApplicationLogging::Normal);
        QVERIFY(logging.available());

        qCInfo(lcTestPlayer) << "default-info-entry";
        qCDebug(lcTestPlayer) << "default-debug-entry";
        logging.flush();

        const QString contents = readAllLogs(logging.logDirectory());
        QVERIFY(contents.contains("default-info-entry"));
        QVERIFY(contents.contains("[showroom.player]"));
        QVERIFY(contents.contains("[thread "));
        QVERIFY(!contents.contains("default-debug-entry"));
    }

    void savedModesAndFileSwitch()
    {
        ScopedLoggingEnvironment environment;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto options = optionsFor(directory.path());

        {
            ApplicationLogging logging(options);
            logging.setMode(ApplicationLogging::PlaybackDiagnostics);
            QVERIFY(logging.dirty());
            QVERIFY(logging.save());

            qCDebug(lcTestPlayer) << "player-debug-entry";
            qCDebug(lcTestRanking) << "ranking-debug-entry";
            qCDebug(lcTestLive) << "live-debug-entry";
            logging.flush();
            const QString contents = readAllLogs(logging.logDirectory());
            QVERIFY(contents.contains("player-debug-entry"));
            QVERIFY(contents.contains("ranking-debug-entry"));
            QVERIFY(!contents.contains("live-debug-entry"));

            logging.setFileEnabled(false);
            QVERIFY(logging.save());
            qCInfo(lcTestPlayer) << "not-written-after-disable";
            logging.flush();
            QVERIFY(!readAllLogs(logging.logDirectory()).contains("not-written-after-disable"));
            QVERIFY(QFile::exists(logging.logDirectory() + QStringLiteral("/ShowroomPlayer.log")));
        }

        {
            ApplicationLogging restored(options);
            QVERIFY(!restored.fileEnabled());
            QCOMPARE(restored.mode(), ApplicationLogging::PlaybackDiagnostics);
            QVERIFY(restored.available());
        }
    }

    void startupOverridesAndCategoryRules()
    {
        ScopedLoggingEnvironment environment;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const auto options = optionsFor(directory.path());
        QDir().mkpath(QFileInfo(options.settingsPath).absolutePath());
        QFile settings(options.settingsPath);
        QVERIFY(settings.open(QIODevice::WriteOnly));
        settings.write(R"({"fileEnabled":true,"mode":"normal","categoryRules":"showroom.live.debug=true"})");
        settings.close();

        qputenv("SHOWROOM_LOG_FILE_ENABLED", "0");
        qputenv("SHOWROOM_LOG_MODE", "playback");
        qputenv("SHOWROOM_LOG_RULES", "showroom.api.debug=true");
        qputenv("QT_LOGGING_RULES", "showroom.controller.debug=true");
        capturedMessages.clear();
        ScopedMessageHandler systemCapture(&captureSystemMessage);
        {
            ApplicationLogging logging(options);
            QVERIFY(!logging.fileEnabled());
            QCOMPARE(logging.mode(), ApplicationLogging::PlaybackDiagnostics);
            QVERIFY(!logging.dirty());

            qCDebug(lcTestController) << "qt-env-rule-before-save";
            {
                QMutexLocker lock(&capturedMessagesMutex);
                QVERIFY(capturedMessages.join(QLatin1Char('\n')).contains("qt-env-rule-before-save"));
            }

            logging.setFileEnabled(true);
            QVERIFY(logging.dirty());
            QVERIFY(logging.save());
            QCOMPARE(logging.mode(), ApplicationLogging::PlaybackDiagnostics);
            QVERIFY(logging.fileEnabled());
            QVERIFY(!logging.dirty());

            QFile persistedSettings(options.settingsPath);
            QVERIFY(persistedSettings.open(QIODevice::ReadOnly));
            const QJsonObject persisted =
                QJsonDocument::fromJson(persistedSettings.readAll()).object();
            QVERIFY(persisted.value(QStringLiteral("fileEnabled")).toBool());
            QCOMPARE(persisted.value(QStringLiteral("mode")).toString(), QStringLiteral("normal"));

            qCDebug(lcTestLive) << "config-category-debug-entry";
            qCDebug(lcTestController) << "qt-env-rule-after-save";
            qCDebug(lcTestApi) << "startup-category-debug-entry";
            logging.flush();
            const QString contents = readAllLogs(logging.logDirectory());
            QVERIFY(contents.contains("config-category-debug-entry"));
            QVERIFY(!contents.contains("qt-env-rule-after-save"));
            QVERIFY(contents.contains("startup-category-debug-entry"));

            {
                QMutexLocker lock(&capturedMessagesMutex);
                QVERIFY(!capturedMessages.join(QLatin1Char('\n')).contains("qt-env-rule-after-save"));
            }

            QCOMPARE(persisted.value(QStringLiteral("categoryRules")).toString(),
                     QStringLiteral("showroom.live.debug=true"));
        }

        qunsetenv("SHOWROOM_LOG_FILE_ENABLED");
        qunsetenv("SHOWROOM_LOG_MODE");
        qunsetenv("SHOWROOM_LOG_RULES");
        {
            ApplicationLogging logging(options);
            QVERIFY(logging.fileEnabled());
            QCOMPARE(logging.mode(), ApplicationLogging::Normal);
            qCDebug(lcTestApi) << "api-debug-after-restart";
            logging.flush();
            QVERIFY(!readAllLogs(logging.logDirectory()).contains("api-debug-after-restart"));
        }
    }

    void sensitiveValuesAreRedactedForFileAndSystemOutput()
    {
        ScopedLoggingEnvironment environment;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        capturedMessages.clear();
        ScopedMessageHandler systemCapture(&captureSystemMessage);

        ApplicationLogging logging(optionsFor(directory.path()));
        qCInfo(lcTestPlayer) << "Authorization: Bearer top-secret-auth-value";
        qCInfo(lcTestPlayer) << "Authorization=Bearer top-secret-equals-token";
        qCInfo(lcTestPlayer) << "Cookie=top-secret-cookie; other=value";
        qCInfo(lcTestPlayer) << "HLS https://media.example/live.m3u8?token=top-secret-query&quality=1";
        qCInfo(lcTestPlayer) << "HLS https://viewer:top-secret-userinfo@media.example/live.m3u8";
        logging.flush();

        const QString fileText = readAllLogs(logging.logDirectory());
        QString systemText;
        {
            QMutexLocker lock(&capturedMessagesMutex);
            systemText = capturedMessages.join(QLatin1Char('\n'));
        }
        QVERIFY(fileText.contains("<REDACTED>"));
        QVERIFY(systemText.contains("<REDACTED>"));
        QVERIFY(!fileText.contains("top-secret-auth-value"));
        QVERIFY(!fileText.contains("top-secret-equals-token"));
        QVERIFY(!fileText.contains("top-secret-cookie"));
        QVERIFY(!fileText.contains("top-secret-query"));
        QVERIFY(!fileText.contains("top-secret-userinfo"));
        QVERIFY(!systemText.contains("top-secret-auth-value"));
        QVERIFY(!systemText.contains("top-secret-equals-token"));
        QVERIFY(!systemText.contains("top-secret-cookie"));
        QVERIFY(!systemText.contains("top-secret-query"));
        QVERIFY(!systemText.contains("top-secret-userinfo"));
    }

    void concurrentMessagesRotate()
    {
        ScopedLoggingEnvironment environment;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ScopedMessageHandler quietSystemOutput(+[](QtMsgType, const QMessageLogContext &, const QString &) {});

        ApplicationLogging logging(optionsFor(directory.path(), 1024, 8192));

        std::vector<std::thread> producers;
        for (int producer = 0; producer < 4; ++producer) {
            producers.emplace_back([producer]() {
                for (int entry = 0; entry < 50; ++entry)
                    qCInfo(lcTestPlayer) << "rotation-entry" << producer << entry
                                         << QString(96, QLatin1Char('x'));
            });
        }
        for (std::thread &producer : producers)
            producer.join();
        logging.flush();

        const QString contents = readAllLogs(logging.logDirectory());
        QVERIFY(contents.contains("rotation-entry"));
        QVERIFY(contents.contains("[showroom.player]"));

        const QStringList files = QDir(logging.logDirectory()).entryList(
            {QStringLiteral("ShowroomPlayer*")}, QDir::Files);
        QCOMPARE(files.size(), 4);
    }

    void queueSaturationReportsDrops()
    {
        ScopedLoggingEnvironment environment;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ScopedMessageHandler quietSystemOutput(+[](QtMsgType, const QMessageLogContext &, const QString &) {});

        ApplicationLogging logging(optionsFor(directory.path(), 10 * 1024 * 1024, 4));
        logging.setMode(ApplicationLogging::PlaybackDiagnostics);
        QVERIFY(logging.save());

        std::vector<std::thread> producers;
        for (int producer = 0; producer < 4; ++producer) {
            producers.emplace_back([producer]() {
                for (int entry = 0; entry < 2500; ++entry)
                    qCDebug(lcTestPlayer) << "concurrent-entry" << producer << entry;
            });
        }
        for (std::thread &producer : producers)
            producer.join();
        logging.flush();

        QVERIFY(logging.droppedMessages() > 0);
        const QString contents = readAllLogs(logging.logDirectory());
        QVERIFY(contents.contains("Logging queue discarded"));
        QVERIFY(contents.contains("[showroom.player]"));
    }

    void unwritableLogDirectoryDoesNotFailConstruction()
    {
        ScopedLoggingEnvironment environment;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        capturedMessages.clear();
        ScopedMessageHandler systemCapture(&captureSystemMessage);

        const QString blocker = directory.filePath(QStringLiteral("not-a-directory"));
        QFile file(blocker);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("block");
        file.close();

        auto options = optionsFor(directory.path());
        options.logDirectory = blocker + QStringLiteral("/logs");
        ApplicationLogging logging(options);
        QVERIFY(logging.fileEnabled());
        QVERIFY(!logging.available());
        QVERIFY(logging.statusMessage().contains("unavailable", Qt::CaseInsensitive));
        {
            QMutexLocker lock(&capturedMessagesMutex);
            QVERIFY(capturedMessages.join(QLatin1Char('\n'))
                        .contains("Could not create log directory", Qt::CaseInsensitive));
        }

        qCInfo(lcTestPlayer) << "app-remains-running";
        logging.flush();
        QCOMPARE(logging.fileEnabled(), true);
    }
};

QTEST_GUILESS_MAIN(ApplicationLoggingTest)
#include "tst_ApplicationLogging.moc"
