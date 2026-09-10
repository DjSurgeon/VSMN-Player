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
option(BUILD_TESTS "Build unit test suite" ON)

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

# Sanitizers configuration
if(ENABLE_SANITIZERS AND ENABLE_TSAN)
    message(FATAL_ERROR "ASan and TSan cannot be compiled together at the same time")
endif()

if(ENABLE_SANITIZERS)
    message(STATUS "AddressSanitizer and UBSan enabled")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# Locate dependencies managed by Conan 2
find_package(spdlog REQUIRED)
find_package(CURL REQUIRED)

# Include public headers
include_directories(include)

# Core library definition
add_library(iptv_core STATIC
    src/pipeline/dummy.cpp
    src/network/http_client.cpp
)
target_include_directories(iptv_core PUBLIC include)
target_compile_options(iptv_core PRIVATE ${PROJECT_WARNINGS})
target_link_libraries(iptv_core PUBLIC spdlog::spdlog CURL::libcurl)

# Main executable
add_executable(iptv_player src/main.cpp)
target_compile_options(iptv_player PRIVATE ${PROJECT_WARNINGS})
target_link_libraries(iptv_player PRIVATE iptv_core)

# Unit tests
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()