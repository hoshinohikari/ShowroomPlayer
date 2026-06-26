#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>

#include "app_environment.h"
#include "import_qml_plugins.h"
#include "ShowroomLog.h"

#include <clocale>

int main(int argc, char *argv[])
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow::setTextRenderType(QQuickWindow::QtTextRendering);

    set_qt_environment();
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");

    qCInfo(lcShowroomApp) << "ShowroomPlayer starting";

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

    qCInfo(lcShowroomApp) << "Main window loaded";
    return app.exec();
}
