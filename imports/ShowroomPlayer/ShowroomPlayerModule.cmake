set_source_files_properties(Constants.qml
    PROPERTIES
        QT_QML_SINGLETON_TYPE true
)

qt_add_library(ShowroomPlayer STATIC)
qt6_add_qml_module(ShowroomPlayer
    URI "ShowroomPlayer"
    VERSION 1.0
    QML_FILES
        Constants.qml
    SOURCES
        ShowroomLog.cpp
        ShowroomLog.h
        MpvVideoItem.cpp
        MpvVideoItem.h
        ShowroomApi.cpp
        ShowroomApi.h
        ShowroomController.cpp
        ShowroomController.h
        UserListModel.cpp
        UserListModel.h
)

target_include_directories(ShowroomPlayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ShowroomPlayer PRIVATE
    Qt6::Quick
    Qt6::OpenGL
    Qt6::Network
    libmpv::libmpv
)

add_custom_command(TARGET ShowroomPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_QML_OUTPUT_DIRECTORY}/ShowroomPlayer/ShowroomPlayer.qmltypes"
        "${CMAKE_CURRENT_SOURCE_DIR}/ShowroomPlayer.qmltypes"
    COMMENT "Sync ShowroomPlayer.qmltypes for Qt Design Studio"
)
