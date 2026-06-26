add_subdirectory(imports/ShowroomPlayer)
add_subdirectory(content)

qt6_add_qml_module(${CMAKE_PROJECT_NAME}
    URI "Main"
    VERSION 1.0
    DEPENDENCIES
        ShowroomPlayer
        content
    QML_FILES Main.qml
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    showroom_contentplugin
    ShowroomPlayerplugin
)
