# MultiCPackConfig.cmake — Cross-Platform CPack Orchestrator
# Modeled on SamuelMarks/premiere-pro-cmake-plugin

set(OPENMEDIA_PACKAGING_ROOT "${CMAKE_CURRENT_LIST_DIR}")

if(WIN32)
    include("${OPENMEDIA_PACKAGING_ROOT}/CPackConfig_Windows.cmake")
elseif(APPLE)
    include("${OPENMEDIA_PACKAGING_ROOT}/CPackConfig_macOS.cmake")
else()
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_NAME "OpenMediaPremierePlugin")
    set(CPACK_PACKAGE_VERSION "2.0.0")
endif()

set(CPACK_RESOURCE_FILE_LICENSE "${OPENMEDIA_PACKAGING_ROOT}/../../THIRD_PARTY_LICENSES.md")
