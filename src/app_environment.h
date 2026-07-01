#ifndef APP_ENVIRONMENT_H
#define APP_ENVIRONMENT_H

#include <QtGlobal>

inline void set_qt_environment()
{
    qputenv("QML_COMPAT_RESOLVE_URLS_ON_ASSIGNMENT", "1");
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");
    qputenv("QT_LOGGING_RULES",
            "qt.qml.connections=false;"
            "showroom.*.debug=true;"
            "showroom.*.info=true");
    qputenv("QT_QUICK_CONTROLS_CONF", ":/qtquickcontrols2.conf");
}

#endif // APP_ENVIRONMENT_H
