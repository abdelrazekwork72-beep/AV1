cmake_minimum_required(VERSION 3.20)
project(OpenMediaPremierePlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" cfg_upper)
    set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_upper}" "${CMAKE_BINARY_DIR}/bin")
    set("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg_upper}" "${CMAKE_BINARY_DIR}/bin")
    set("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg_upper}" "${CMAKE_BINARY_DIR}/lib")
endforeach()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerWarnings.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindAdobeSDK.cmake")

function(openmedia_enable_test_assertions target_name)
    if(MSVC)
        target_compile_options("${target_name}" PRIVATE /UNDEBUG)
    else()
        target_compile_options("${target_name}" PRIVATE -UNDEBUG)
    endif()
endfunction()

function(openmedia_resolve_ffmpeg out_found out_libs out_incs)
    find_package(FFMPEG QUIET)
    if(FFMPEG_FOUND)
        set(${out_found} TRUE PARENT_SCOPE)
        set(${out_libs} ${FFMPEG_LIBRARIES} PARENT_SCOPE)
        set(${out_incs} ${FFMPEG_INCLUDE_DIRS} PARENT_SCOPE)
        return()
    endif()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(OPENMEDIA_FFMPEG QUIET IMPORTED_TARGET
            libavformat libavcodec libswscale libswresample libavutil)
    endif()

    if(TARGET PkgConfig::OPENMEDIA_FFMPEG)
        set(${out_found} TRUE PARENT_SCOPE)
        set(${out_libs} PkgConfig::OPENMEDIA_FFMPEG PARENT_SCOPE)
        set(${out_incs} "" PARENT_SCOPE)
        return()
    endif()

    message(STATUS "FFmpeg not found; configuring OpenMediaCore in mock/stub mode.")
    set(${out_found} FALSE PARENT_SCOPE)
    set(${out_libs} "" PARENT_SCOPE)
    set(${out_incs} "" PARENT_SCOPE)
endfunction()

function(openmedia_configure_plugin_platform target_name)
    if(WIN32)
        enable_language(RC OPTIONAL)
        find_program(OPENMEDIA_RC_TOOL NAMES llvm-rc windres rc rc.exe)
        set_target_properties("${target_name}" PROPERTIES
            PREFIX ""
            SUFFIX ".prm"
        )
        set(rc_OpenMedia_Importer "${CMAKE_CURRENT_SOURCE_DIR}/resources/importer_resources.rc")
        set(rc_OpenMedia_ImporterHP "${CMAKE_CURRENT_SOURCE_DIR}/resources/importer_hp_resources.rc")
        set(rc_OpenMedia_Exporter "${CMAKE_CURRENT_SOURCE_DIR}/resources/exporter_resources.rc")
        set(rc_file "${rc_${target_name}}")
        if(NOT EXISTS "${rc_file}")
            set(rc_file "${CMAKE_CURRENT_SOURCE_DIR}/resources/${target_name}.rc")
        endif()
        if(EXISTS "${rc_file}")
            target_sources("${target_name}" PRIVATE "${rc_file}")
        endif()
    elseif(APPLE)
        set_target_properties("${target_name}" PROPERTIES
            BUNDLE TRUE
            SUFFIX ".bundle"
            MACOSX_BUNDLE_BUNDLE_NAME "${target_name}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.openmedia.premiere.${target_name}"
        )
        set(plist_OpenMedia_Importer "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info_Importer.plist.in")
        set(plist_OpenMedia_ImporterHP "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info_ImporterHP.plist.in")
        set(plist_OpenMedia_Exporter "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info_Exporter.plist.in")
        set(plist_in "${plist_${target_name}}")
        if(NOT EXISTS "${plist_in}")
            set(plist_in "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info_${target_name}.plist.in")
        endif()
        if(NOT EXISTS "${plist_in}")
            set(plist_in "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in")
        endif()
        if(EXISTS "${plist_in}")
            set_target_properties("${target_name}" PROPERTIES
                MACOSX_BUNDLE_INFO_PLIST "${plist_in}"
            )
        endif()
        set(pkginfo_file "${CMAKE_CURRENT_BINARY_DIR}/generated/${target_name}_PkgInfo")
        file(GENERATE OUTPUT "${pkginfo_file}" CONTENT "BNDL????")
        add_custom_command(TARGET "${target_name}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${pkginfo_file}"
                "$<TARGET_BUNDLE_CONTENT_DIR:${target_name}>/PkgInfo"
            COMMENT "Generating PkgInfo for ${target_name}"
        )
    endif()
endfunction()

function(openmedia_add_plugin_target target_name source_subdir)
    file(GLOB plugin_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${source_subdir}/*.cpp"
    )
    if(NOT plugin_sources)
        set(stub_file "${CMAKE_CURRENT_BINARY_DIR}/generated/${target_name}_stub.cpp")
        if(NOT EXISTS "${stub_file}")
            file(WRITE "${stub_file}" "// Auto-generated placeholder until src/${source_subdir} is populated.\n")
        endif()
        set(plugin_sources "${stub_file}")
    endif()

    add_library("${target_name}_obj" OBJECT ${plugin_sources})
    target_include_directories("${target_name}_obj" PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries("${target_name}_obj" PUBLIC OpenMediaCore)
    if(TARGET AdobeSDK::Headers)
        target_link_libraries("${target_name}_obj" PUBLIC AdobeSDK::Headers)
    endif()
    if(TARGET AdobeSDK::Utils)
        target_link_libraries("${target_name}_obj" PUBLIC AdobeSDK::Utils)
    endif()
    if(OPENMEDIA_FFMPEG_FOUND)
        target_compile_definitions("${target_name}_obj" PRIVATE OPENMEDIA_HAS_FFMPEG=1)
        target_include_directories("${target_name}_obj" PRIVATE ${OPENMEDIA_FFMPEG_INCS})
        target_link_libraries("${target_name}_obj" PUBLIC ${OPENMEDIA_FFMPEG_LIBS})
    endif()
    openmedia_apply_warnings("${target_name}_obj")

    add_library("${target_name}" MODULE $<TARGET_OBJECTS:${target_name}_obj>)
    target_link_libraries("${target_name}" PRIVATE OpenMediaCore)
    if(TARGET AdobeSDK::Headers)
        target_link_libraries("${target_name}" PRIVATE AdobeSDK::Headers)
    endif()
    if(TARGET AdobeSDK::Utils)
        target_link_libraries("${target_name}" PRIVATE AdobeSDK::Utils)
    endif()
    if(OPENMEDIA_FFMPEG_FOUND)
        target_link_libraries("${target_name}" PRIVATE ${OPENMEDIA_FFMPEG_LIBS})
    endif()
    openmedia_apply_warnings("${target_name}")
    openmedia_configure_plugin_platform("${target_name}")
endfunction()

add_library(OpenMediaCore SHARED
    src/core/demuxer.cpp
    src/core/video_decoder.cpp
    src/core/audio_decoder.cpp
    src/core/video_encoder.cpp
    src/core/muxer.cpp
)
target_include_directories(OpenMediaCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
openmedia_apply_warnings(OpenMediaCore)

if(WIN32)
    set_target_properties(OpenMediaCore PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

openmedia_resolve_ffmpeg(OPENMEDIA_FFMPEG_FOUND OPENMEDIA_FFMPEG_LIBS OPENMEDIA_FFMPEG_INCS)
if(OPENMEDIA_FFMPEG_FOUND)
    target_compile_definitions(OpenMediaCore PUBLIC OPENMEDIA_HAS_FFMPEG=1)
    target_include_directories(OpenMediaCore PRIVATE ${OPENMEDIA_FFMPEG_INCS})
    target_link_libraries(OpenMediaCore PRIVATE ${OPENMEDIA_FFMPEG_LIBS})
endif()

openmedia_add_plugin_target(OpenMedia_Importer "importer")
openmedia_add_plugin_target(OpenMedia_ImporterHP "importer")
target_compile_definitions(OpenMedia_ImporterHP_obj PRIVATE OPENMEDIA_IMPORTER_HIGH_PRIORITY=1)
target_compile_definitions(OpenMedia_ImporterHP PRIVATE OPENMEDIA_IMPORTER_HIGH_PRIORITY=1)
openmedia_add_plugin_target(OpenMedia_Exporter "exporter")

enable_testing()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/build/test_cmake_targets.cmake")
    add_test(
        NAME test_cmake_targets
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/build/test_cmake_targets.cmake"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/build/test_vcpkg_manifest.py")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME test_vcpkg_manifest
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/build/test_vcpkg_manifest.py"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/build/test_find_adobe_sdk.cmake")
    add_test(
        NAME test_find_adobe_sdk
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/build/test_find_adobe_sdk.cmake"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/core")
    file(GLOB OPENMEDIA_CORE_TESTS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/core/test_*.cpp"
    )
    foreach(test_src IN LISTS OPENMEDIA_CORE_TESTS)
        get_filename_component(test_name "${test_src}" NAME_WE)
        add_executable("${test_name}" "${test_src}")
        target_link_libraries("${test_name}" PRIVATE OpenMediaCore)
        openmedia_apply_warnings("${test_name}")
        openmedia_enable_test_assertions("${test_name}")
        add_test(
            NAME "${test_name}"
            COMMAND "${test_name}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endforeach()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/importer")
    file(GLOB OPENMEDIA_IMPORTER_TESTS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/importer/test_*.cpp"
    )
    foreach(test_src IN LISTS OPENMEDIA_IMPORTER_TESTS)
        get_filename_component(test_name "${test_src}" NAME_WE)
        add_executable("${test_name}" "${test_src}")
        target_link_libraries("${test_name}" PRIVATE OpenMedia_Importer_obj OpenMediaCore)
        openmedia_apply_warnings("${test_name}")
        openmedia_enable_test_assertions("${test_name}")
        add_test(
            NAME "${test_name}"
            COMMAND "${test_name}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endforeach()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/exporter")
    file(GLOB OPENMEDIA_EXPORTER_TESTS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exporter/test_*.cpp"
    )
    foreach(test_src IN LISTS OPENMEDIA_EXPORTER_TESTS)
        get_filename_component(test_name "${test_src}" NAME_WE)
        add_executable("${test_name}" "${test_src}")
        target_link_libraries("${test_name}" PRIVATE OpenMedia_Exporter_obj OpenMediaCore)
        openmedia_apply_warnings("${test_name}")
        openmedia_enable_test_assertions("${test_name}")
        add_test(
            NAME "${test_name}"
            COMMAND "${test_name}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endforeach()
endif()

# Module 4: MediaCore Host Integration & Benchmark Harness (US-4.1.0)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration")
    add_library(openmedia_integration_harness STATIC
        tests/integration/src/deployment_validator.cpp
        tests/integration/src/scrub_benchmark.cpp
        tests/integration/src/ame_queue_validator.cpp
    )
    target_include_directories(openmedia_integration_harness PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks/mock_sdk/Examples/Headers"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(openmedia_integration_harness PUBLIC
        OpenMedia_Importer_obj
        OpenMedia_Exporter_obj
        OpenMediaCore
    )
    if(UNIX AND NOT APPLE)
        target_link_libraries(openmedia_integration_harness PUBLIC ${CMAKE_DL_LIBS})
    endif()
    openmedia_apply_warnings(openmedia_integration_harness)
    foreach(integ_test IN ITEMS
        test_deployment_validator
        test_scrub_benchmark
        test_ame_queue_validator
    )
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/${integ_test}.cpp")
            add_executable("${integ_test}" "tests/integration/${integ_test}.cpp")
            target_link_libraries("${integ_test}" PRIVATE
                openmedia_integration_harness
                OpenMedia_Importer_obj
                OpenMedia_Exporter_obj
                OpenMediaCore
            )
            openmedia_apply_warnings("${integ_test}")
            openmedia_enable_test_assertions("${integ_test}")
            add_test(
                NAME "${integ_test}"
                COMMAND "${integ_test}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            )
        endif()
    endforeach()
endif()

# Module 4: Cross-Platform CPack Packaging & License Audit (US-4.2.0)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/packaging/test_windows_cpack.cmake")
    add_test(
        NAME test_windows_cpack
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/packaging/test_windows_cpack.cmake"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/packaging/test_macos_cpack.cmake")
    add_test(
        NAME test_macos_cpack
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/packaging/test_macos_cpack.cmake"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/packaging/test_license_audit.py")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME test_license_audit
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/packaging/test_license_audit.py"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endif()
endif()

install(TARGETS OpenMediaCore OpenMedia_Importer OpenMedia_ImporterHP OpenMedia_Exporter
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
    BUNDLE DESTINATION .
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_LICENSES.md")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_LICENSES.md" DESTINATION .)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/cpack/MultiCPackConfig.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/packaging/cpack/MultiCPackConfig.cmake")
    include(CPack)
endif()
