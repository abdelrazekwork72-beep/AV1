# test_find_adobe_sdk.cmake
# CTest script (TC-1.1.3) verifying:
#   1. FATAL_ERROR when -DADOBE_PP_SDK is omitted
#   2. Exit 0 and exported targets when tests/mocks/mock_sdk is supplied

get_filename_component(_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(_FIND_MODULE "${_REPO_ROOT}/cmake/FindAdobeSDK.cmake")
set(_MOCK_SDK "${_REPO_ROOT}/tests/mocks/mock_sdk")
set(_TEST_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/_tc_1_1_3_tmp")

file(REMOVE_RECURSE "${_TEST_WORK_ROOT}")

# Case 1: Omitted -DADOBE_PP_SDK must emit FATAL_ERROR
set(_NEG_SRC "${_TEST_WORK_ROOT}/neg_src")
set(_NEG_BIN "${_TEST_WORK_ROOT}/neg_bin")
file(MAKE_DIRECTORY "${_NEG_SRC}")
file(WRITE "${_NEG_SRC}/CMakeLists.txt" "cmake_minimum_required(VERSION 3.20)
project(TestMissingAdobeSDK LANGUAGES NONE)
include(${_FIND_MODULE})
")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_NEG_SRC}" -B "${_NEG_BIN}"
    RESULT_VARIABLE _NEG_RESULT
    OUTPUT_VARIABLE _NEG_STDOUT
    ERROR_VARIABLE _NEG_STDERR
)

if(_NEG_RESULT EQUAL 0)
    file(REMOVE_RECURSE "${_TEST_WORK_ROOT}")
    message(FATAL_ERROR "TC-1.1.3 FAILED: Expected non-zero exit code when -DADOBE_PP_SDK is omitted, got 0.")
endif()

if(NOT _NEG_STDERR MATCHES "-DADOBE_PP_SDK=/path/to/Premiere_Pro_SDK")
    file(REMOVE_RECURSE "${_TEST_WORK_ROOT}")
    message(FATAL_ERROR "TC-1.1.3 FAILED: Expected -DADOBE_PP_SDK=/path/to/Premiere_Pro_SDK in stderr, got: ${_NEG_STDERR}")
endif()

# Case 2: Valid mock_sdk must succeed with exit 0 and export targets
set(_POS_SRC "${_TEST_WORK_ROOT}/pos_src")
set(_POS_BIN "${_TEST_WORK_ROOT}/pos_bin")
file(MAKE_DIRECTORY "${_POS_SRC}")
file(WRITE "${_POS_SRC}/CMakeLists.txt" "cmake_minimum_required(VERSION 3.20)
project(TestMockAdobeSDK LANGUAGES NONE)
include(${_FIND_MODULE})
if(NOT TARGET AdobeSDK::Headers OR NOT TARGET AdobeSDK::Utils)
    message(FATAL_ERROR Missing_AdobeSDK_targets)
endif()
")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_POS_SRC}" -B "${_POS_BIN}" "-DADOBE_PP_SDK=${_MOCK_SDK}"
    RESULT_VARIABLE _POS_RESULT
    OUTPUT_VARIABLE _POS_STDOUT
    ERROR_VARIABLE _POS_STDERR
)

file(REMOVE_RECURSE "${_TEST_WORK_ROOT}")

if(NOT _POS_RESULT EQUAL 0)
    message(FATAL_ERROR "TC-1.1.3 FAILED: Expected exit 0 with mock_sdk, got ${_POS_RESULT}. STDERR: ${_POS_STDERR}")
endif()

message(STATUS "TC-1.1.3 PASSED: FindAdobeSDK.cmake rejected missing -DADOBE_PP_SDK and resolved mock_sdk.")
