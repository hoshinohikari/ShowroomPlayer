#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QScreen>
#include <QCoreApplication>
#include <QDir>
#include <QIcon>
#if !defined(Q_OS_MACOS)
#include <QSurfaceFormat>
#endif

#include "app_environment.h"
#include "import_qml_plugins.h"
#include "ShowroomLog.h"
#include "version.h"

#include <clocale>

namespace {

#if !defined(Q_OS_MACOS)

void configureOpenGLSurfaceFormat()
{
    QSurfaceFormat fmt;
    fmt.setVersion(3, 2);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);
}

#endif

QIcon loadAppIcon()
{
#if defined(Q_OS_MACOS)
    const QString iconPath =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../Resources/icon.icns"));
    const QIcon bundleIcon(iconPath);
    if (!bundleIcon.isNull())
        return bundleIcon;
#endif
    return QIcon(QStringLiteral("qrc:/qt/qml/content/icon/icon.ico"));
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QQuickWindow::setTextRenderType(QQuickWindow::QtTextRendering);
#if defined(Q_OS_MACOS)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#else
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    configureOpenGLSurfaceFormat();
#endif

    set_qt_environment();

    QCoreApplication::setApplicationName(QStringLiteral("ShowroomPlayer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SHOWROOM_VERSION_STRING));

    QGuiApplication app(argc, argv);
    const QIcon appIcon = loadAppIcon();
    if (!appIcon.isNull())
        app.setWindowIcon(appIcon);
    std::setlocale(LC_NUMERIC, "C");

    qCInfo(lcShowroomApp) << "ShowroomPlayer starting, version" << SHOWROOM_VERSION_STRING;

    if (const QScreen *screen = app.primaryScreen()) {
        qCInfo(lcShowroomApp) << "Primary screen DPR:" << screen->devicePixelRatio()
                              << "logical DPI:" << screen->logicalDotsPerInch()
                              << "size:" << screen->size();
    }

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::quit, &app, &QGuiApplication::quit);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            qCCritical(lcShowroomApp) << "QML object creation failed";
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.loadFromModule("Main", "Main");

    if (engine.rootObjects().isEmpty()) {
        qCCritical(lcShowroomApp) << "Failed to load Main QML module";
        return -1;
    }

    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
        if (!appIcon.isNull())
            window->setIcon(appIcon);
    }

    qCInfo(lcShowroomApp) << "Main window loaded";
    return app.exec();
}
