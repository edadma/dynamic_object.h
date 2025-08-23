cmake_minimum_required(VERSION 3.10)
project(dynamic_object C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add compiler flags for debugging and warnings
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Default to Release build
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# Example executable
add_executable(tests tests.c libs/unity/unity.c)

enable_testing()
add_test(NAME tests COMMAND test)

# Optional: Installation
install(FILES dynamic_object.h
        DESTINATION include
        COMPONENT Development)

# Print build information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")