# CTest script verifying OpenMediaCore, OpenMedia_Importer, and OpenMedia_Exporter
# CMake target configuration and platform-specific .prm / .bundle suffix rules.

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SOURCE_DIR)
    get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()
if(NOT DEFINED BINARY_DIR)
    set(BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
endif()

function(assert_file_contains file_path pattern description)
    file(READ "${file_path}" file_content)
    string(FIND "${file_content}" "${pattern}" match_index)
    if(match_index EQUAL -1)
        message(FATAL_ERROR "Assertion failed (${description}): '${pattern}' not found in ${file_path}")
    endif()
endfunction()

assert_file_contains("${SOURCE_DIR}/cmake/CompilerWarnings.cmake" "openmedia_apply_warnings" "warning function")
assert_file_contains("${SOURCE_DIR}/cmake/CompilerWarnings.cmake" "-Wall -Wextra -Wpedantic" "GCC/Clang warnings")
assert_file_contains("${SOURCE_DIR}/cmake/CompilerWarnings.cmake" "/W4" "MSVC warnings")
assert_file_contains("${SOURCE_DIR}/CMakeLists.txt" "SUFFIX \".prm\"" "Windows .prm suffix")
assert_file_contains("${SOURCE_DIR}/CMakeLists.txt" "SUFFIX \".bundle\"" "macOS .bundle suffix")
assert_file_contains("${SOURCE_DIR}/CMakeLists.txt" "BUNDLE TRUE" "macOS bundle property")

set(TEST_BUILD_DIR "${BINARY_DIR}/_test_cmake_targets_build")
file(REMOVE_RECURSE "${TEST_BUILD_DIR}")
file(MAKE_DIRECTORY "${TEST_BUILD_DIR}/.cmake/api/v1/query")
file(TOUCH "${TEST_BUILD_DIR}/.cmake/api/v1/query/codemodel-v2")

set(MOCK_SDK_DIR "${SOURCE_DIR}/tests/mocks/mock_sdk")
if(NOT EXISTS "${MOCK_SDK_DIR}")
    set(MOCK_SDK_DIR "${TEST_BUILD_DIR}/mock_sdk_fallback")
    file(MAKE_DIRECTORY "${MOCK_SDK_DIR}/Examples/Headers")
    file(MAKE_DIRECTORY "${MOCK_SDK_DIR}/Examples/Utils")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${SOURCE_DIR}"
        -B "${TEST_BUILD_DIR}"
        "-DADOBE_PP_SDK=${MOCK_SDK_DIR}"
    RESULT_VARIABLE cfg_result
    OUTPUT_VARIABLE cfg_stdout
    ERROR_VARIABLE cfg_stderr
)
if(NOT cfg_result EQUAL 0)
    message(FATAL_ERROR "CMake configure failed (${cfg_result}):\n${cfg_stdout}\n${cfg_stderr}")
endif()

file(GLOB TARGET_JSON_FILES "${TEST_BUILD_DIR}/.cmake/api/v1/reply/target-*.json")
if(NOT TARGET_JSON_FILES)
    message(FATAL_ERROR "CMake File API did not produce any target reply JSON files.")
endif()

set(FOUND_CORE FALSE)
set(FOUND_IMPORTER FALSE)
set(FOUND_EXPORTER FALSE)

foreach(json_file IN LISTS TARGET_JSON_FILES)
    file(READ "${json_file}" json_text)
    string(JSON target_name GET "${json_text}" "name")
    string(JSON target_type GET "${json_text}" "type")
    string(JSON name_on_disk ERROR_VARIABLE disk_err GET "${json_text}" "nameOnDisk")

    if(target_name STREQUAL "OpenMediaCore")
        set(FOUND_CORE TRUE)
        if(NOT target_type STREQUAL "SHARED_LIBRARY")
            message(FATAL_ERROR "Expected OpenMediaCore to be SHARED_LIBRARY, got ${target_type}")
        endif()
    elseif(target_name STREQUAL "OpenMedia_Importer")
        set(FOUND_IMPORTER TRUE)
        if(NOT target_type STREQUAL "MODULE_LIBRARY")
            message(FATAL_ERROR "Expected OpenMedia_Importer to be MODULE_LIBRARY, got ${target_type}")
        endif()
        if(WIN32 AND NOT name_on_disk STREQUAL "OpenMedia_Importer.prm")
            message(FATAL_ERROR "Expected OpenMedia_Importer.prm on WIN32, got ${name_on_disk}")
        elseif(APPLE AND NOT name_on_disk MATCHES "OpenMedia_Importer\\.bundle")
            message(FATAL_ERROR "Expected .bundle suffix for OpenMedia_Importer on APPLE, got ${name_on_disk}")
        endif()
    elseif(target_name STREQUAL "OpenMedia_Exporter")
        set(FOUND_EXPORTER TRUE)
        if(NOT target_type STREQUAL "MODULE_LIBRARY")
            message(FATAL_ERROR "Expected OpenMedia_Exporter to be MODULE_LIBRARY, got ${target_type}")
        endif()
        if(WIN32 AND NOT name_on_disk STREQUAL "OpenMedia_Exporter.prm")
            message(FATAL_ERROR "Expected OpenMedia_Exporter.prm on WIN32, got ${name_on_disk}")
        elseif(APPLE AND NOT name_on_disk MATCHES "OpenMedia_Exporter\\.bundle")
            message(FATAL_ERROR "Expected .bundle suffix for OpenMedia_Exporter on APPLE, got ${name_on_disk}")
        endif()
    endif()
endforeach()

if(NOT FOUND_CORE OR NOT FOUND_IMPORTER OR NOT FOUND_EXPORTER)
    message(FATAL_ERROR "Missing expected targets: Core=${FOUND_CORE}, Importer=${FOUND_IMPORTER}, Exporter=${FOUND_EXPORTER}")
endif()

file(REMOVE_RECURSE "${TEST_BUILD_DIR}")
message(STATUS "test_cmake_targets: PASSED")
