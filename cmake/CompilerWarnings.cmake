# Compiler warning, SIMD, LTCG hardening, and security configuration for OpenMedia targets.

include_guard(GLOBAL)
include(CheckCXXCompilerFlag)

function(openmedia_apply_warnings target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "openmedia_apply_warnings: '${target_name}' is not a valid CMake target.")
    endif()

    if(MSVC)
        target_compile_options("${target_name}" PRIVATE /W4 /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options("${target_name}" PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(openmedia_apply_release_hardening target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "openmedia_apply_release_hardening: '${target_name}' is not a valid CMake target.")
    endif()

    get_target_property(target_type "${target_name}" TYPE)
    get_target_property(export_all_syms "${target_name}" WINDOWS_EXPORT_ALL_SYMBOLS)

    if(MSVC)
        target_compile_options("${target_name}" PRIVATE $<$<CONFIG:Release>:/O2>)
        if(export_all_syms)
            set_target_properties("${target_name}" PROPERTIES
                INTERPROCEDURAL_OPTIMIZATION FALSE
                INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE
            )
        elseif(NOT target_type STREQUAL "OBJECT_LIBRARY")
            target_compile_options("${target_name}" PRIVATE $<$<CONFIG:Release>:/GL>)
            target_link_options("${target_name}" PRIVATE $<$<CONFIG:Release>:/LTCG>)
        endif()
    endif()
endfunction()

function(openmedia_apply_simd_and_security target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "openmedia_apply_simd_and_security: '${target_name}' is not a valid CMake target.")
    endif()

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
        if(MSVC)
            target_compile_options("${target_name}" PRIVATE /arch:AVX2)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
            check_cxx_compiler_flag("-mavx2" OPENMEDIA_HAS_MAVX2)
            check_cxx_compiler_flag("-msse4.1" OPENMEDIA_HAS_MSSE41)
            if(OPENMEDIA_HAS_MAVX2)
                target_compile_options("${target_name}" PRIVATE -mavx2)
            endif()
            if(OPENMEDIA_HAS_MSSE41)
                target_compile_options("${target_name}" PRIVATE -msse4.1)
            endif()
        endif()
    endif()

    get_target_property(target_type "${target_name}" TYPE)
    if(WIN32 AND NOT target_type STREQUAL "OBJECT_LIBRARY")
        if(MSVC)
            target_link_options("${target_name}" PRIVATE /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)
        elseif(MINGW)
            target_link_options("${target_name}" PRIVATE -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va)
        endif()
    endif()

    openmedia_apply_release_hardening("${target_name}")
endfunction()
