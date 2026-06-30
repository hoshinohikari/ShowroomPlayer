# Finalize a macOS app bundle after post-build deploy steps.
# Qt's POST_BUILD deploy creates QML plugin symlinks outside the bundle, and
# install_name_tool invalidates the linker signature. Both cause macOS to show
# a generic "prohibited" app icon until the bundle is repaired and re-signed.

if(NOT DEFINED BUNDLE_DIR)
    message(FATAL_ERROR "BUNDLE_DIR is required")
endif()

file(GLOB _framework_dylibs "${BUNDLE_DIR}/Contents/Frameworks/*.dylib")

file(GLOB_RECURSE _qml_entries "${BUNDLE_DIR}/Contents/Resources/qml/*")
foreach(_entry IN LISTS _qml_entries)
    if(IS_SYMLINK "${_entry}")
        file(READ_SYMLINK "${_entry}" _link_target)
        if(IS_ABSOLUTE "${_link_target}" AND EXISTS "${_link_target}")
            get_filename_component(_link_dir "${_entry}" DIRECTORY)
            file(REMOVE "${_entry}")
            file(COPY "${_link_target}" DESTINATION "${_link_dir}")
        endif()
    endif()
endforeach()

foreach(_dylib IN LISTS _framework_dylibs)
    execute_process(
        COMMAND codesign --force --sign - "${_dylib}"
        RESULT_VARIABLE _result
        ERROR_VARIABLE _error
    )
    if(NOT _result EQUAL 0)
        message(WARNING "codesign failed for ${_dylib}: ${_error}")
    endif()
endforeach()

set(_executable "${BUNDLE_DIR}/Contents/MacOS/ShowroomPlayer")
execute_process(
    COMMAND codesign --force --sign - "${_executable}"
    RESULT_VARIABLE _result
    ERROR_VARIABLE _error
)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "codesign failed for ${_executable}: ${_error}")
endif()
