
cmake_minimum_required(VERSION 3.21.1)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(ShowroomPlayerApp VERSION 1.1.0.1 LANGUAGES CXX)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
    @ONLY
)

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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick OpenGL Network QuickControls2 WebSockets)

qt_policy(SET QTP0005 NEW)
qt_policy(SET QTP0004 NEW)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Libmpv.cmake)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(${CMAKE_PROJECT_NAME}
    WIN32
    MACOSX_BUNDLE
    src/main.cpp
)

if(WIN32)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        content/icon/app.rc
    )
elseif(APPLE)
    set(_app_icon "${CMAKE_CURRENT_SOURCE_DIR}/content/icon/icon.icns")
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${_app_icon}")
    set_source_files_properties("${_app_icon}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

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
    ${CMAKE_CURRENT_BINARY_DIR}/generated
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
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(APPLE)
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE icon)
endif()

libmpv_copy_runtime_dll(${CMAKE_PROJECT_NAME})

qt_generate_deploy_qml_app_script(
    TARGET ${CMAKE_PROJECT_NAME}
    OUTPUT_SCRIPT deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT ${deploy_script})

if(APPLE)
    add_custom_command(
        TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/content/icon/icon.icns"
            "$<TARGET_BUNDLE_CONTENT_DIR:${CMAKE_PROJECT_NAME}>/Resources/icon.icns"
        COMMENT "Ensure app icon remains in bundle after Qt deploy"
    )
    add_custom_command(
        TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -D "BUNDLE_DIR=$<TARGET_BUNDLE_DIR:${CMAKE_PROJECT_NAME}>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/finalize_macos_bundle.cmake"
        COMMENT "Repair and re-sign macOS app bundle"
    )
endif()
