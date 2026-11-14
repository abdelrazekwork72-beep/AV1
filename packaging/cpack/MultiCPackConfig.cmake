# MultiCPackConfig.cmake — Cross-Platform CPack Orchestrator
# Modeled on SamuelMarks/premiere-pro-cmake-plugin

set(OPENMEDIA_PACKAGING_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(OPENMEDIA_REPO_ROOT "${OPENMEDIA_PACKAGING_ROOT}/../.." ABSOLUTE)

# Run pre-packaging SPDX dual-license audit and generate THIRD_PARTY_LICENSES.md when Python3 is available
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}"
            "${OPENMEDIA_PACKAGING_ROOT}/../scripts/generate_third_party_notices.py"
            "--output" "${OPENMEDIA_REPO_ROOT}/THIRD_PARTY_LICENSES.md"
        RESULT_VARIABLE OPENMEDIA_NOTICE_RC
    )
    if(NOT OPENMEDIA_NOTICE_RC EQUAL 0)
        message(FATAL_ERROR "generate_third_party_notices.py failed with exit code ${OPENMEDIA_NOTICE_RC}")
    endif()

    execute_process(
        COMMAND "${Python3_EXECUTABLE}"
            "${OPENMEDIA_PACKAGING_ROOT}/../scripts/audit_licenses.py"
            "--root" "${OPENMEDIA_REPO_ROOT}"
        RESULT_VARIABLE OPENMEDIA_AUDIT_RC
    )
    if(NOT OPENMEDIA_AUDIT_RC EQUAL 0)
        message(FATAL_ERROR "audit_licenses.py failed with exit code ${OPENMEDIA_AUDIT_RC}")
    endif()
endif()

if(WIN32)
    include("${OPENMEDIA_PACKAGING_ROOT}/CPackConfig_Windows.cmake")
elseif(APPLE)
    include("${OPENMEDIA_PACKAGING_ROOT}/CPackConfig_macOS.cmake")
else()
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_NAME "OpenMediaPremierePlugin")
    set(CPACK_PACKAGE_VERSION "1.0.0")
endif()

if(EXISTS "${OPENMEDIA_REPO_ROOT}/THIRD_PARTY_LICENSES.md")
    set(CPACK_RESOURCE_FILE_LICENSE "${OPENMEDIA_REPO_ROOT}/THIRD_PARTY_LICENSES.md")
endif()
