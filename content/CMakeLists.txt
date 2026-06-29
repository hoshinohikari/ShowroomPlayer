set_source_files_properties(Theme.qml
    PROPERTIES
        QT_QML_SINGLETON_TYPE true
)

qt_add_library(showroom_content STATIC)
qt6_add_qml_module(showroom_content
    URI "content"
    DEPENDENCIES
        ShowroomPlayer
    QML_FILES
        App.qml
        Theme.qml
        components/DarkTextField.qml
        components/AccentButton.qml
        components/SecondaryButton.qml
        components/IconOverlayButton.qml
        components/SplitHandle.qml
        panels/LoginDialog.qml
        panels/SidebarPanel.qml
        panels/VideoPanel.qml
        panels/ChatPanel.qml
    RESOURCES
        icon/icon.ico
)
