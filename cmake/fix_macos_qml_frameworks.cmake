# Copy Qt frameworks that macdeployqt failed to pull in.
#
# macdeployqt (Qt 6.10) does not always deploy the Qt framework dependencies
# of QML style plugins (e.g. QtQuickControls2Impl, QtQuickControls2Basic,
# QtQuickLayouts, QtQuickShapes, ...). The QML plugin dylibs end up in
# Contents/Resources/qml with @rpath references to frameworks that are not in
# Contents/Frameworks, so dlopen fails at startup and QML reports the plugin as
# "not found".
#
# This script scans every binary already deployed in the bundle for
# @rpath/Qt*.framework references, copies any missing ones from the Qt install
# (whose framework dylibs already use @rpath install ids, so the main binary's
# @executable_path/../Frameworks rpath resolves them with no install_name_tool
# needed), and ad-hoc signs the copied framework executables.

if(NOT DEFINED BUNDLE_DIR)
    message(FATAL_ERROR "BUNDLE_DIR is required")
endif()
if(NOT DEFINED QT_INSTALL_LIBS)
    message(FATAL_ERROR "QT_INSTALL_LIBS is required (Qt6 lib dir containing *.framework)")
endif()

set(_fw_dir "${BUNDLE_DIR}/Contents/Frameworks")
set(_qml_dir "${BUNDLE_DIR}/Contents/Resources/qml")

# Binaries whose @rpath/Qt*.framework references we should satisfy.
file(GLOB_RECURSE _binaries
    "${BUNDLE_DIR}/Contents/MacOS/*"
    "${_qml_dir}/*.dylib"
    "${_fw_dir}/Qt*.framework/Versions/*/*"
)

set(_needed_fws "")
foreach(_bin IN LISTS _binaries)
    execute_process(
        COMMAND otool -L "${_bin}"
        OUTPUT_VARIABLE _otool_out
        ERROR_QUIET
        RESULT_VARIABLE _res
    )
    if(NOT _res EQUAL 0)
        continue()
    endif()
    string(REPLACE "\n" ";" _lines "${_otool_out}")
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "@rpath/(Qt[A-Za-z0-9_]+)\\.framework")
            list(APPEND _needed_fws "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endforeach()

list(REMOVE_DUPLICATES _needed_fws)

set(_copied 0)
foreach(_fw IN LISTS _needed_fws)
    set(_dst "${_fw_dir}/${_fw}.framework")
    if(EXISTS "${_dst}")
        continue()
    endif()
    set(_src "${QT_INSTALL_LIBS}/${_fw}.framework")
    if(NOT EXISTS "${_src}")
        continue()
    endif()

    message(STATUS "Copying missing Qt framework: ${_fw}")
    execute_process(
        COMMAND cp -RH "${_src}" "${_fw_dir}/"
        RESULT_VARIABLE _cp_res
        ERROR_VARIABLE _cp_err
    )
    if(NOT _cp_res EQUAL 0)
        message(WARNING "cp failed for ${_fw}: ${_cp_err}")
        continue()
    endif()

    # Ad-hoc sign the framework's versioned executable (follows Versions/Current).
    file(GLOB _fw_executables "${_dst}/Versions/*/${_fw}")
    foreach(_exe IN LISTS _fw_executables)
        execute_process(
            COMMAND codesign --force --sign - "${_exe}"
            RESULT_VARIABLE _sign_res
            ERROR_VARIABLE _sign_err
        )
        if(NOT _sign_res EQUAL 0)
            message(WARNING "codesign failed for ${_exe}: ${_sign_err}")
        endif()
    endforeach()

    math(EXPR _copied "${_copied} + 1")
endforeach()

message(STATUS "fix_macos_qml_frameworks: copied ${_copied} missing Qt framework(s)")
