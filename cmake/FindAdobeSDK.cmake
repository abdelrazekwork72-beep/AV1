# FindAdobeSDK.cmake
# Resolves external Adobe Premiere Pro SDK path passed via -DADOBE_PP_SDK
# (including mock SDK via -DADOBE_PP_SDK=tests/mocks/mock_sdk).
# Mitigates CWE-22 path traversal via REALPATH canonicalization and exports
# AdobeSDK::Headers, AdobeSDK::Utils, AdobeSDK::Importer, and
# AdobeSDK::ImporterHP INTERFACE targets for dual-PRM shim builds.

if(NOT DEFINED ADOBE_PP_SDK OR "${ADOBE_PP_SDK}" STREQUAL "")
    message(FATAL_ERROR
        "ADOBE_PP_SDK is required but was not provided. "
        "Please configure CMake with: -DADOBE_PP_SDK=/path/to/Premiere_Pro_SDK"
    )
endif()

# Canonicalize path relative to project source root (CWE-22 mitigation)
if(DEFINED CMAKE_SOURCE_DIR AND NOT "${CMAKE_SOURCE_DIR}" STREQUAL "")
    get_filename_component(_ADOBE_SDK_REALPATH "${ADOBE_PP_SDK}" REALPATH BASE_DIR "${CMAKE_SOURCE_DIR}")
else()
    get_filename_component(_ADOBE_SDK_REALPATH "${ADOBE_PP_SDK}" REALPATH)
endif()

if(NOT IS_DIRECTORY "${_ADOBE_SDK_REALPATH}")
    message(FATAL_ERROR
        "ADOBE_PP_SDK path does not exist or is not a directory: '${_ADOBE_SDK_REALPATH}'. "
        "Please configure CMake with: -DADOBE_PP_SDK=/path/to/Premiere_Pro_SDK"
    )
endif()

set(_ADOBE_SDK_HEADERS_DIR "${_ADOBE_SDK_REALPATH}/Examples/Headers")
set(_ADOBE_SDK_UTILS_DIR "${_ADOBE_SDK_REALPATH}/Examples/Utils")

if(NOT IS_DIRECTORY "${_ADOBE_SDK_HEADERS_DIR}" OR NOT IS_DIRECTORY "${_ADOBE_SDK_UTILS_DIR}")
    message(FATAL_ERROR
        "Invalid Adobe Premiere Pro SDK directory structure at '${_ADOBE_SDK_REALPATH}'. "
        "Expected 'Examples/Headers' and 'Examples/Utils' subdirectories. "
        "Please configure CMake with: -DADOBE_PP_SDK=/path/to/Premiere_Pro_SDK"
    )
endif()

if(NOT TARGET AdobeSDK::Headers)
    add_library(AdobeSDK::Headers INTERFACE IMPORTED)
    set_target_properties(AdobeSDK::Headers PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_ADOBE_SDK_HEADERS_DIR}"
        INTERFACE_COMPILE_FEATURES "cxx_std_17"
    )
endif()

if(NOT TARGET AdobeSDK::Utils)
    add_library(AdobeSDK::Utils INTERFACE IMPORTED)
    set_target_properties(AdobeSDK::Utils PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_ADOBE_SDK_UTILS_DIR}"
        INTERFACE_LINK_LIBRARIES "AdobeSDK::Headers"
    )
endif()

if(NOT TARGET AdobeSDK::Importer)
    add_library(AdobeSDK::Importer INTERFACE IMPORTED)
    set_target_properties(AdobeSDK::Importer PROPERTIES
        INTERFACE_COMPILE_FEATURES "cxx_std_17"
        INTERFACE_LINK_LIBRARIES "AdobeSDK::Headers;AdobeSDK::Utils"
    )
endif()

if(NOT TARGET AdobeSDK::ImporterHP)
    add_library(AdobeSDK::ImporterHP INTERFACE IMPORTED)
    set_target_properties(AdobeSDK::ImporterHP PROPERTIES
        INTERFACE_COMPILE_FEATURES "cxx_std_17"
        INTERFACE_COMPILE_DEFINITIONS "OPENMEDIA_IMPORTER_HIGH_PRIORITY=1"
        INTERFACE_LINK_LIBRARIES "AdobeSDK::Headers;AdobeSDK::Utils"
    )
endif()

if(NOT COMMAND openmedia_configure_dual_prm_sdk)
    function(openmedia_configure_dual_prm_sdk target_name)
        if(NOT TARGET "${target_name}")
            message(FATAL_ERROR "openmedia_configure_dual_prm_sdk: '${target_name}' is not a valid target.")
        endif()
        if("${target_name}" MATCHES "ImporterHP")
            target_link_libraries("${target_name}" PUBLIC AdobeSDK::ImporterHP)
        elseif("${target_name}" MATCHES "Importer")
            target_link_libraries("${target_name}" PUBLIC AdobeSDK::Importer)
        else()
            target_link_libraries("${target_name}" PUBLIC AdobeSDK::Headers AdobeSDK::Utils)
        endif()
    endfunction()
endif()

if(EXISTS "${_ADOBE_SDK_HEADERS_DIR}/PrSDKImport.h")
    set(ADOBE_SDK_HAS_PRSDK_IMPORT TRUE)
else()
    set(ADOBE_SDK_HAS_PRSDK_IMPORT FALSE)
endif()

set(AdobeSDK_FOUND TRUE)
set(ADOBE_SDK_FOUND TRUE)
set(ADOBE_SDK_ROOT "${_ADOBE_SDK_REALPATH}")
set(ADOBE_SDK_HEADERS_DIR "${_ADOBE_SDK_HEADERS_DIR}")
set(ADOBE_SDK_UTILS_DIR "${_ADOBE_SDK_UTILS_DIR}")
set(ADOBE_SDK_DUAL_PRM_SUPPORTED TRUE)
set(ADOBE_SDK_DUAL_PRM_TARGETS "OpenMedia_Importer;OpenMedia_ImporterHP")

message(STATUS "Found Adobe Premiere Pro SDK: ${_ADOBE_SDK_REALPATH}")
