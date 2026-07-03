# Finalize a macOS app bundle after post-build deploy steps.
#
# Qt's POST_BUILD deploy creates QML plugin symlinks outside the bundle, and
# install_name_tool invalidates the linker signature. Both cause macOS to show
# a generic "prohibited" app icon until the bundle is repaired and re-signed.
#
# Two additional problems we handle here:
#   1. Qt frameworks ship with a top-level `Headers` symlink that breaks the
#      "sealed root" requirement for embedded frameworks (codesign rejects it
#      with "unsealed contents present in the root directory"). We strip it.
#   2. Some bundle transports (zip via actions/upload-artifact, third-party
#      archive utilities) flatten framework symlinks into real copies, which
#      produces duplicate content under Versions/A and Versions/Current and
#      breaks codesign's seal. We restore the canonical symlink layout before
#      signing. The script is idempotent: if symlinks are already present we
#      leave them alone.
#
# Signing uses --options runtime + an entitlements plist that disables library
# validation, which is required for ad-hoc signed Qt apps that load ad-hoc
# signed Qt frameworks and QML plugin dylibs at runtime.

if(NOT DEFINED BUNDLE_DIR)
    message(FATAL_ERROR "BUNDLE_DIR is required")
endif()

set(_executable "${BUNDLE_DIR}/Contents/MacOS/ShowroomPlayer")
set(_fw_dir "${BUNDLE_DIR}/Contents/Frameworks")
set(_qml_dir "${BUNDLE_DIR}/Contents/Resources/qml")

# --- Restore canonical framework symlink layout ---------------------------

file(GLOB _frameworks "${_fw_dir}/*.framework")
foreach(_fw IN LISTS _frameworks)
    get_filename_component(_fw_name "${_fw}" NAME_WE)

    # Versions/Current must be a symlink to the versioned dir (usually "A").
    if(EXISTS "${_fw}/Versions/Current" AND NOT IS_SYMLINK "${_fw}/Versions/Current")
        file(REMOVE_RECURSE "${_fw}/Versions/Current")
        execute_process(COMMAND ln -s A "${_fw}/Versions/Current")
    endif()

    # Top-level executable symlink -> Versions/Current/<name>.
    if(EXISTS "${_fw}/${_fw_name}" AND NOT IS_SYMLINK "${_fw}/${_fw_name}")
        file(REMOVE "${_fw}/${_fw_name}")
        execute_process(COMMAND ln -s "Versions/Current/${_fw_name}" "${_fw}/${_fw_name}")
    endif()

    # Top-level Resources symlink -> Versions/Current/Resources.
    if(EXISTS "${_fw}/Resources" AND NOT IS_SYMLINK "${_fw}/Resources")
        file(REMOVE_RECURSE "${_fw}/Resources")
        execute_process(COMMAND ln -s "Versions/Current/Resources" "${_fw}/Resources")
    endif()

    # Top-level Headers (symlink or dir) breaks the embedded-framework seal.
    if(EXISTS "${_fw}/Headers")
        file(REMOVE_RECURSE "${_fw}/Headers")
    endif()
    file(GLOB _ver_headers "${_fw}/Versions/*/Headers")
    foreach(_vh IN LISTS _ver_headers)
        file(REMOVE_RECURSE "${_vh}")
    endforeach()
endforeach()

# --- Resolve QML plugin symlinks into real files (bundle must be self-contained) ---

file(GLOB_RECURSE _qml_entries "${_qml_dir}/*")
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

# --- Write entitlements plist -------------------------------------------
# Hardened runtime + disabled library validation lets an ad-hoc signed main
# binary load ad-hoc signed Qt frameworks and QML plugin dylibs without being
# killed by AMFI for "team ID mismatch".

set(_ents_plist "${BUNDLE_DIR}/../showroom_entitlements.plist")
file(WRITE "${_ents_plist}" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
file(APPEND "${_ents_plist}" "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n")
file(APPEND "${_ents_plist}" "<plist version=\"1.0\">\n<dict>\n")
file(APPEND "${_ents_plist}" "  <key>com.apple.security.cs.allow-jit</key><true/>\n")
file(APPEND "${_ents_plist}" "  <key>com.apple.security.cs.allow-unsigned-executable-memory</key><true/>\n")
file(APPEND "${_ents_plist}" "  <key>com.apple.security.cs.disable-library-validation</key><true/>\n")
file(APPEND "${_ents_plist}" "  <key>com.apple.security.cs.allow-dyld-environment-variables</key><true/>\n")
file(APPEND "${_ents_plist}" "</dict>\n</plist>\n")

# --- Re-sign from the inside out -----------------------------------------

# 1. Standalone .dylib files in Contents/Frameworks (e.g. Qt private helpers).
file(GLOB _framework_dylibs "${_fw_dir}/*.dylib")
foreach(_dylib IN LISTS _framework_dylibs)
    execute_process(
        COMMAND codesign --force --sign - --options runtime "${_dylib}"
        RESULT_VARIABLE _result
        ERROR_VARIABLE _error
    )
    if(NOT _result EQUAL 0)
        message(WARNING "codesign failed for ${_dylib}: ${_error}")
    endif()
endforeach()

# 2. PlugIns dylibs.
file(GLOB_RECURSE _plugin_dylibs "${BUNDLE_DIR}/Contents/PlugIns/*.dylib")
foreach(_dylib IN LISTS _plugin_dylibs)
    execute_process(
        COMMAND codesign --force --sign - --options runtime "${_dylib}"
        RESULT_VARIABLE _result
        ERROR_VARIABLE _error
    )
    if(NOT _result EQUAL 0)
        message(WARNING "codesign failed for ${_dylib}: ${_error}")
    endif()
endforeach()

# 3. Framework bundles (each .framework is itself a bundle that must be signed).
foreach(_fw IN LISTS _frameworks)
    execute_process(
        COMMAND codesign --force --sign - --options runtime "${_fw}"
        RESULT_VARIABLE _result
        ERROR_VARIABLE _error
    )
    if(NOT _result EQUAL 0)
        message(WARNING "codesign failed for ${_fw}: ${_error}")
    endif()
endforeach()

# 4. Main executable with entitlements.
execute_process(
    COMMAND codesign --force --sign - --options runtime
            --entitlements "${_ents_plist}" "${_executable}"
    RESULT_VARIABLE _result
    ERROR_VARIABLE _error
)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "codesign failed for ${_executable}: ${_error}")
endif()

# 5. Outer bundle (seals Info.plist, Resources, etc.).
execute_process(
    COMMAND codesign --force --sign - --options runtime
            --entitlements "${_ents_plist}" "${BUNDLE_DIR}"
    RESULT_VARIABLE _result
    ERROR_VARIABLE _error
)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "codesign failed for ${BUNDLE_DIR}: ${_error}")
endif()

# Final verification — non-fatal warning so a stale CodeResources doesn't
# abort the whole build, but surfaces real structural issues.
execute_process(
    COMMAND codesign --verify --deep --strict "${BUNDLE_DIR}"
    RESULT_VARIABLE _verify_result
    ERROR_VARIABLE _verify_error
    OUTPUT_VARIABLE _verify_output
)
if(NOT _verify_result EQUAL 0)
    message(WARNING "codesign verify reported issues (often harmless for ad-hoc builds): ${_verify_error}")
else()
    message(STATUS "finalize_macos_bundle: bundle signed and verified")
endif()
