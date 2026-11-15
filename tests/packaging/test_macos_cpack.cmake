# test_macos_cpack.cmake — CTest validation for TC-4.2.2
if(NOT DEFINED SOURCE_DIR)
    get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

set(mac_cfg "${SOURCE_DIR}/packaging/cpack/CPackConfig_macOS.cmake")
set(dist_xml "${SOURCE_DIR}/packaging/cpack/macos/distribution.xml")
set(post_sh "${SOURCE_DIR}/packaging/cpack/macos/postinstall.sh")
set(cfg_toml "${SOURCE_DIR}/packaging/Config.toml")

foreach(req_file IN ITEMS "${mac_cfg}" "${dist_xml}" "${post_sh}" "${cfg_toml}")
    if(NOT EXISTS "${req_file}")
        message(FATAL_ERROR "Missing macOS packaging file: ${req_file}")
    endif()
endforeach()

include("${mac_cfg}")

if(NOT OPENMEDIA_MACOS_MEDIACORE_DIR STREQUAL "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore")
    message(FATAL_ERROR "Invalid macOS MediaCore 7.0 target path: ${OPENMEDIA_MACOS_MEDIACORE_DIR}")
endif()

foreach(expected_artifact IN ITEMS "OpenMedia_Importer.bundle" "OpenMedia_ImporterHP.bundle" "OpenMedia_Exporter.bundle" "libOpenMediaCore.dylib" "Config.toml")
    list(FIND OPENMEDIA_MACOS_ARTIFACTS "${expected_artifact}" idx)
    if(idx EQUAL -1)
        message(FATAL_ERROR "CPackConfig_macOS.cmake missing ${expected_artifact}")
    endif()
endforeach()

file(READ "${post_sh}" post_content)
if(NOT post_content MATCHES "set -euo pipefail")
    message(FATAL_ERROR "postinstall.sh missing 'set -euo pipefail'")
endif()
if(post_content MATCHES "eval ")
    message(FATAL_ERROR "postinstall.sh must not contain eval")
endif()
if(NOT post_content MATCHES "OpenMedia_ImporterHP.bundle" OR NOT post_content MATCHES "Config.toml")
    message(FATAL_ERROR "postinstall.sh missing OpenMedia_ImporterHP.bundle or Config.toml")
endif()
if(NOT post_content MATCHES "0755" OR NOT post_content MATCHES "0644")
    message(FATAL_ERROR "postinstall.sh missing 0755/0644 permission enforcement")
endif()

message(STATUS "[PASS] TC-4.2.2 macOS CPack, Dual-PRM + Config.toml, & hardened postinstall.sh verified.")
