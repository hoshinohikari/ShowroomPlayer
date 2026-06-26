qt_add_library(showroom_content STATIC)
qt6_add_qml_module(showroom_content
    URI "content"
    DEPENDENCIES
        ShowroomPlayer
    QML_FILES
        App.qml
)
