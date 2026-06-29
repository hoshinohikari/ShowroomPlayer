#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QScreen>
#include <QCoreApplication>
#include <QIcon>

#include "app_environment.h"
#include "import_qml_plugins.h"
#include "ShowroomLog.h"
#include "version.h"

#include <clocale>

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow::setTextRenderType(QQuickWindow::QtTextRendering);

    set_qt_environment();

    QCoreApplication::setApplicationName(QStringLiteral("ShowroomPlayer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SHOWROOM_VERSION_STRING));

    QGuiApplication app(argc, argv);
    const QIcon appIcon(QStringLiteral("qrc:/qt/qml/content/icon/icon.ico"));
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

    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()))
        window->setIcon(appIcon);

    qCInfo(lcShowroomApp) << "Main window loaded";
    return app.exec();
}
