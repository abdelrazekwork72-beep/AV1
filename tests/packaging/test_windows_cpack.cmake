# test_windows_cpack.cmake — CTest validation for TC-4.2.1
if(NOT DEFINED SOURCE_DIR)
    get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

set(win_cfg "${SOURCE_DIR}/packaging/cpack/CPackConfig_Windows.cmake")
set(wix_patch "${SOURCE_DIR}/packaging/cpack/windows/wix_patch.xml")

if(NOT EXISTS "${win_cfg}")
    message(FATAL_ERROR "Missing CPackConfig_Windows.cmake at ${win_cfg}")
endif()
if(NOT EXISTS "${wix_patch}")
    message(FATAL_ERROR "Missing wix_patch.xml at ${wix_patch}")
endif()

include("${win_cfg}")

if(NOT OPENMEDIA_WIN_MEDIACORE_DIR MATCHES "Adobe\\\\Common\\\\Plug-ins\\\\7\\.0\\\\MediaCore")
    message(FATAL_ERROR "Invalid Windows MediaCore 7.0 target path: ${OPENMEDIA_WIN_MEDIACORE_DIR}")
endif()

if(NOT CPACK_WIX_INSTALL_SCOPE STREQUAL "perMachine")
    message(FATAL_ERROR "Expected perMachine WiX scope for CWE-732 protection")
endif()

foreach(req_bin IN ITEMS "OpenMedia_Importer.prm" "OpenMedia_Exporter.prm" "libOpenMediaCore.dll")
    if(NOT "${req_bin}" IN_LIST OPENMEDIA_WINDOWS_ARTIFACTS)
        message(FATAL_ERROR "Missing Windows artifact in OPENMEDIA_WINDOWS_ARTIFACTS: ${req_bin}")
    endif()
endforeach()

file(READ "${wix_patch}" wix_content)
if(NOT wix_content MATCHES "ALLUSERS" OR NOT wix_content MATCHES "Privileged")
    message(FATAL_ERROR "wix_patch.xml missing Administrator/ALLUSERS enforcement")
endif()

message(STATUS "[PASS] TC-4.2.1 Windows CPack & WiX MediaCore 7.0 configuration verified.")
