# Integrate a prebuilt libmpv package (include/, libmpv-2.dll, import library).
#
# Expected layout under LIBMPV_ROOT (default: ${CMAKE_SOURCE_DIR}/libmpv):
#   include/mpv/client.h
#   libmpv-2.dll
#   libmpv-2.lib (MSVC) or libmpv.dll.a (MinGW)

set(LIBMPV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libmpv" CACHE PATH "Path to prebuilt libmpv")

find_path(
    LIBMPV_INCLUDE_DIR
    NAMES mpv/client.h
    HINTS "${LIBMPV_ROOT}/include"
    REQUIRED
)

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

function(libmpv_copy_runtime_dll target)
    if(NOT WIN32)
        return()
    endif()
    add_custom_command(
        TARGET "${target}" POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LIBMPV_DLL}"
            "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Copy libmpv-2.dll next to ${target}"
    )
endfunction()

message(STATUS "libmpv include: ${LIBMPV_INCLUDE_DIR}")
message(STATUS "libmpv dll:     ${LIBMPV_DLL}")
message(STATUS "libmpv implib:  ${LIBMPV_IMPLIB}")
