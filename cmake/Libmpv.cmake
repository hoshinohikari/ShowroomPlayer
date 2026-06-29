# Integrate a prebuilt libmpv package.
#
# Expected layout under LIBMPV_ROOT (default: ${CMAKE_SOURCE_DIR}/libmpv):
#   include/mpv/client.h
#   Windows: libmpv-2.dll + import library (libmpv-2.lib / libmpv.dll.a)
#   macOS:   libmpv.dylib (+ dependency .dylib files in the same directory)

set(LIBMPV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libmpv" CACHE PATH "Path to prebuilt libmpv")

find_path(
    LIBMPV_INCLUDE_DIR
    NAMES mpv/client.h
    HINTS "${LIBMPV_ROOT}/include"
    REQUIRED
)

if(APPLE)
    find_file(
        LIBMPV_LIBRARY
        NAMES libmpv.dylib
        HINTS "${LIBMPV_ROOT}"
        REQUIRED
    )

    file(GLOB LIBMPV_DYLIBS CONFIGURE_DEPENDS "${LIBMPV_ROOT}/*.dylib")

    add_library(libmpv::libmpv SHARED IMPORTED GLOBAL)
    set_target_properties(libmpv::libmpv PROPERTIES
        IMPORTED_LOCATION "${LIBMPV_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBMPV_INCLUDE_DIR}"
    )
elseif(WIN32)
    find_file(
        LIBMPV_DLL
        NAMES libmpv-2.dll mpv-2.dll
        HINTS "${LIBMPV_ROOT}" "${LIBMPV_ROOT}/bin"
        REQUIRED
    )

    set(_libmpv_implib_candidates "")
    if(MSVC)
        list(APPEND _libmpv_implib_candidates
            "${LIBMPV_ROOT}/libmpv-2.lib"
            "${LIBMPV_ROOT}/mpv.lib"
            "${LIBMPV_ROOT}/lib/mpv.lib"
            "${LIBMPV_ROOT}/lib/libmpv-2.lib"
        )
    else()
        list(APPEND _libmpv_implib_candidates
            "${LIBMPV_ROOT}/libmpv.dll.a"
            "${LIBMPV_ROOT}/libmpv-2.dll.a"
            "${LIBMPV_ROOT}/lib/libmpv.dll.a"
            "${LIBMPV_ROOT}/libmpv-2.lib"
            "${LIBMPV_ROOT}/mpv.lib"
        )
    endif()

    set(LIBMPV_IMPLIB "")
    foreach(_candidate IN LISTS _libmpv_implib_candidates)
        if(EXISTS "${_candidate}")
            set(LIBMPV_IMPLIB "${_candidate}")
            break()
        endif()
    endforeach()

    if(MSVC AND NOT LIBMPV_IMPLIB)
        set(_generated_implib "${LIBMPV_ROOT}/libmpv-2.lib")
        if(NOT EXISTS "${_generated_implib}")
            find_package(Python3 COMPONENTS Interpreter REQUIRED)
            execute_process(
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_libmpv_import_lib.py"
                    --dll "${LIBMPV_DLL}"
                    --out-dir "${LIBMPV_ROOT}"
                RESULT_VARIABLE _libmpv_gen_result
                OUTPUT_VARIABLE _libmpv_gen_output
                ERROR_VARIABLE _libmpv_gen_error
            )
            if(NOT _libmpv_gen_result EQUAL 0)
                message(FATAL_ERROR
                    "MSVC import library missing for libmpv.\n"
                    "${_libmpv_gen_output}\n"
                    "${_libmpv_gen_error}\n"
                    "Place libmpv-2.lib in ${LIBMPV_ROOT}, or install Visual Studio Build Tools "
                    "so lib.exe can generate it from libmpv-2.dll."
                )
            endif()
        endif()
        if(EXISTS "${_generated_implib}")
            set(LIBMPV_IMPLIB "${_generated_implib}")
        endif()
    endif()

    if(NOT LIBMPV_IMPLIB)
        message(FATAL_ERROR
            "libmpv import library not found under ${LIBMPV_ROOT}.\n"
            "MSVC: libmpv-2.lib or mpv.lib\n"
            "MinGW: libmpv.dll.a"
        )
    endif()

    add_library(libmpv::libmpv SHARED IMPORTED GLOBAL)
    set_target_properties(libmpv::libmpv PROPERTIES
        IMPORTED_LOCATION "${LIBMPV_DLL}"
        IMPORTED_IMPLIB "${LIBMPV_IMPLIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBMPV_INCLUDE_DIR}"
    )
else()
    message(FATAL_ERROR "libmpv prebuilt package is only supported on Windows and macOS.")
endif()

function(libmpv_copy_runtime_dll target)
    if(WIN32)
        add_custom_command(
            TARGET "${target}" POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LIBMPV_DLL}"
                "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Copy libmpv-2.dll next to ${target}"
        )
    elseif(APPLE)
        set(_frameworks_dir "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks")

        add_custom_command(
            TARGET "${target}" POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_frameworks_dir}"
            COMMENT "Create Frameworks directory for libmpv"
        )

        foreach(_dylib IN LISTS LIBMPV_DYLIBS)
            get_filename_component(_dylib_name "${_dylib}" NAME)
            add_custom_command(
                TARGET "${target}" POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_dylib}"
                    "${_frameworks_dir}/${_dylib_name}"
                COMMENT "Copy ${_dylib_name} into app bundle"
            )
        endforeach()

        add_custom_command(
            TARGET "${target}" POST_BUILD
            COMMAND install_name_tool
                -change "${LIBMPV_LIBRARY}"
                "@rpath/libmpv.dylib"
                "$<TARGET_FILE:${target}>"
            COMMENT "Retarget libmpv to @rpath in app binary"
        )

        add_custom_command(
            TARGET "${target}" POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -D "FRAMEWORKS_DIR=${_frameworks_dir}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fixup_libmpv_rpath.cmake"
            COMMENT "Add @loader_path rpath to bundled libmpv dylibs"
        )

        set_target_properties("${target}" PROPERTIES
            MACOSX_RPATH ON
            BUILD_RPATH "@executable_path/../Frameworks"
            INSTALL_RPATH "@executable_path/../Frameworks"
        )
    endif()
endfunction()

if(APPLE)
    message(STATUS "libmpv include:  ${LIBMPV_INCLUDE_DIR}")
    message(STATUS "libmpv library: ${LIBMPV_LIBRARY}")
    message(STATUS "libmpv dylibs:  ${LIBMPV_DYLIBS}")
else()
    message(STATUS "libmpv include: ${LIBMPV_INCLUDE_DIR}")
    message(STATUS "libmpv dll:     ${LIBMPV_DLL}")
    message(STATUS "libmpv implib:  ${LIBMPV_IMPLIB}")
endif()
