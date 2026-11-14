# FindAdobeSDK.cmake
# Resolves external Adobe Premiere Pro SDK path passed via -DADOBE_PP_SDK.
# Mitigates CWE-22 path traversal via REALPATH canonicalization and exports
# AdobeSDK::Headers and AdobeSDK::Utils INTERFACE targets.

if(NOT DEFINED ADOBE_PP_SDK OR "${ADOBE_PP_SDK}" STREQUAL "")
    message(FATAL_ERROR
        "ADOBE_PP_SDK is required but was not provided. "
        "Please configure CMake with: -DADOBE_PP_SDK=/path/to/Premiere_Pro_SDK"
    )
endif()

# Canonicalize path (CWE-22 mitigation)
get_filename_component(_ADOBE_SDK_REALPATH "${ADOBE_PP_SDK}" REALPATH)

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

set(AdobeSDK_FOUND TRUE)
set(ADOBE_SDK_FOUND TRUE)
set(ADOBE_SDK_ROOT "${_ADOBE_SDK_REALPATH}")
set(ADOBE_SDK_HEADERS_DIR "${_ADOBE_SDK_HEADERS_DIR}")
set(ADOBE_SDK_UTILS_DIR "${_ADOBE_SDK_UTILS_DIR}")

message(STATUS "Found Adobe Premiere Pro SDK: ${_ADOBE_SDK_REALPATH}")
