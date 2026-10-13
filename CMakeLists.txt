cmake_minimum_required(VERSION 3.22)
project(iptv_player VERSION 0.1.0 LANGUAGES CXX)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options for quality and sanitizers
option(ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)
option(ENABLE_FUZZER "Enable libFuzzer target" OFF)
option(BUILD_TESTS "Build unit test suite" ON)
option(ENABLE_STRESS_TESTS "Enable massive stress tests" OFF)
option(ENABLE_BENCHMARKS "Enable Google Benchmark suite" OFF)

# Strict warnings flags
set(PROJECT_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wshadow
)

if(ENABLE_WARNINGS_AS_ERRORS)
    list(APPEND PROJECT_WARNINGS -Werror)
endif()

# Global Project Configuration Interface
add_library(project_config INTERFACE)
target_include_directories(project_config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(project_config INTERFACE ${PROJECT_WARNINGS})

# Sanitizers configuration
if(ENABLE_SANITIZERS AND ENABLE_TSAN)
    message(FATAL_ERROR "ASan and TSan cannot be compiled together at the same time")
endif()

if(ENABLE_SANITIZERS)
    message(STATUS "AddressSanitizer and UBSan enabled")
    target_compile_options(project_config INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(project_config INTERFACE -fsanitize=address,undefined)
endif()

if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    target_compile_options(project_config INTERFACE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(project_config INTERFACE -fsanitize=thread)
endif()

# Coverage configuration
if(ENABLE_COVERAGE)
    message(STATUS "Code Coverage enabled")
    target_compile_options(project_config INTERFACE --coverage -O0 -g)
    target_link_options(project_config INTERFACE --coverage)
endif()

# Locate dependencies managed by Conan 2
find_package(CURL REQUIRED)

# Include source directories
add_subdirectory(src)

# Unit tests
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()

# Fuzzer target
if(ENABLE_FUZZER)
    add_executable(iptv_fuzzer tests/fuzz/fuzzer.cpp)
    target_include_directories(iptv_fuzzer PRIVATE include)
    target_compile_options(iptv_fuzzer PRIVATE -fsanitize=address,undefined -g -O1)
    target_link_options(iptv_fuzzer PRIVATE -fsanitize=address,undefined)
    target_link_libraries(iptv_fuzzer PRIVATE iptv_core)
endif()

# Benchmarks
if(ENABLE_BENCHMARKS)
    find_package(benchmark REQUIRED)
    add_subdirectory(benchmarks)
endif()