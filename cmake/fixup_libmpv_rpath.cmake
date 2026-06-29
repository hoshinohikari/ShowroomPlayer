# Add @loader_path rpath to bundled libmpv dylibs (ignore if already present).
if(NOT DEFINED FRAMEWORKS_DIR)
    message(FATAL_ERROR "FRAMEWORKS_DIR is required")
endif()

file(GLOB _dylibs "${FRAMEWORKS_DIR}/*.dylib")
foreach(_dylib IN LISTS _dylibs)
    execute_process(
        COMMAND install_name_tool -add_rpath @loader_path/ "${_dylib}"
        RESULT_VARIABLE _result
        ERROR_QUIET
        OUTPUT_QUIET
    )
endforeach()
