# Compiler warning configuration for OpenMedia targets.

include_guard(GLOBAL)

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
