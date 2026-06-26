
cmake_minimum_required(VERSION 3.21.1)

project(ShowroomPlayerApp VERSION 0.1 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(QT_QML_GENERATE_QMLLS_INI ON)
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)
set(QML_IMPORT_PATH
    "${CMAKE_BINARY_DIR}/qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/imports"
    "${CMAKE_CURRENT_SOURCE_DIR}/content"
    CACHE STRING "Import paths for Qt Creator's code model"
    FORCE
)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick OpenGL Network QuickControls2)

qt_policy(SET QTP0005 NEW)
qt_policy(SET QTP0004 NEW)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Libmpv.cmake)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(${CMAKE_PROJECT_NAME}
    WIN32
    MACOSX_BUNDLE
    src/main.cpp
)

qt_add_resources(${CMAKE_PROJECT_NAME} "configuration"
    PREFIX "/"
    FILES
        qtquickcontrols2.conf
)

qt6_add_resources(${CMAKE_PROJECT_NAME} MainResource
    PREFIX "/qt/qml"
    VERSION 1.0
    FILES
        mpv/sub/osd_font.otf
)

include(qmlmodules.cmake)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/imports/ShowroomPlayer
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
)

include(GNUInstallDirs)
install(TARGETS ${CMAKE_PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ShowroomPlayer
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

libmpv_copy_runtime_dll(${CMAKE_PROJECT_NAME})

qt_generate_deploy_qml_app_script(
    TARGET ${CMAKE_PROJECT_NAME}
    OUTPUT_SCRIPT deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT ${deploy_script})
