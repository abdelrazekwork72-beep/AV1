# test_windows_cpack.cmake — CTest validation for TC-4.2.1 (v2.0.0 NSIS .exe + WiX)
if(NOT DEFINED SOURCE_DIR)
    get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

set(win_cfg "${SOURCE_DIR}/packaging/cpack/CPackConfig_Windows.cmake")
set(wix_patch "${SOURCE_DIR}/packaging/cpack/windows/wix_patch.xml")
set(nsi_script "${SOURCE_DIR}/packaging/cpack/windows/openmedia_installer.nsi")
set(cfg_toml "${SOURCE_DIR}/packaging/Config.toml")

foreach(req_file IN ITEMS "${win_cfg}" "${wix_patch}" "${nsi_script}" "${cfg_toml}")
    if(NOT EXISTS "${req_file}")
        message(FATAL_ERROR "Missing Windows packaging file: ${req_file}")
    endif()
endforeach()

include("${win_cfg}")

if(NOT CPACK_PACKAGE_VERSION_MAJOR STREQUAL "2")
    message(FATAL_ERROR "Expected CPACK_PACKAGE_VERSION_MAJOR 2, got ${CPACK_PACKAGE_VERSION_MAJOR}")
endif()

if(NOT OPENMEDIA_WIN_MEDIACORE_DIR MATCHES "Adobe\\\\Common\\\\Plug-ins\\\\7\\.0\\\\MediaCore")
    message(FATAL_ERROR "Invalid Windows MediaCore 7.0 target path: ${OPENMEDIA_WIN_MEDIACORE_DIR}")
endif()

foreach(expected_artifact IN ITEMS "OpenMedia_Importer.prm" "OpenMedia_ImporterHP.prm" "OpenMedia_Exporter.prm" "OpenMediaCore.dll" "Config.toml" "THIRD_PARTY_LICENSES.md" "README.md")
    list(FIND OPENMEDIA_WINDOWS_ARTIFACTS "${expected_artifact}" idx)
    if(idx EQUAL -1)
        message(FATAL_ERROR "CPackConfig_Windows.cmake missing ${expected_artifact}")
    endif()
endforeach()

if(NOT CPACK_WIX_INSTALL_SCOPE STREQUAL "perMachine")
    message(FATAL_ERROR "Expected perMachine WiX scope for CWE-732 protection")
endif()

file(READ "${wix_patch}" wix_content)
if(NOT wix_content MATCHES "ALLUSERS" OR NOT wix_content MATCHES "Privileged")
    message(FATAL_ERROR "wix_patch.xml missing Administrator/ALLUSERS enforcement")
endif()
if(NOT wix_content MATCHES "VCRUNTIME140.dll" OR NOT wix_content MATCHES "VCRUNTIME140_1.dll")
    message(FATAL_ERROR "wix_patch.xml missing VCRUNTIME140.dll / VCRUNTIME140_1.dll check")
endif()

file(READ "${nsi_script}" nsi_content)
foreach(nsi_token IN ITEMS "RequestExecutionLevel admin" "VCRUNTIME140.dll" "VCRUNTIME140_1.dll" "OpenMedia_Importer.prm" "OpenMedia_ImporterHP.prm" "OpenMedia_Exporter.prm" "OpenMediaCore.dll" "Config.toml" "Uninstall-OpenMedia.exe")
    if(NOT nsi_content MATCHES "${nsi_token}")
        message(FATAL_ERROR "openmedia_installer.nsi missing required token: ${nsi_token}")
    endif()
endforeach()

message(STATUS "[PASS] TC-4.2.1 Windows CPack v2.0.0, NSIS .exe installer, Dual-PRM + Config.toml, & VC++ Runtime check verified.")
