cmake_minimum_required(VERSION 3.15)

project(trading-helloworld
    VERSION 1.0.0
    LANGUAGES CXX
)

include(FetchContent)

# ============================================================================
# Dependencies
# ============================================================================

FetchContent_Declare(
    cpp_cache
    GIT_REPOSITORY https://github.com/tobantal/cpp-cache.git
    GIT_TAG v0.0.1
)

FetchContent_Declare(
    cpp_http_server
    GIT_REPOSITORY https://github.com/tobantal/cpp-http-server.git
    GIT_TAG v0.0.5
)

FetchContent_MakeAvailable(
    cpp_cache
    cpp_http_server
)

find_package(Threads REQUIRED)

# ============================================================================
# Application
# ============================================================================

add_executable(trading-helloworld
    src/main.cpp
    src/HelloWorldApp.cpp
    src/handlers/HealthCheckHandler.cpp
    src/handlers/MetricsHandler.cpp
    src/handlers/EchoHandler.cpp
)

target_include_directories(trading-helloworld
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================================================
# Link libraries (ТОЛЬКО targets!)
# ============================================================================

target_link_libraries(trading-helloworld
    PRIVATE
        microservice-core
        microservice-boost
        cpp_cache
        Threads::Threads
)

# ============================================================================
# Diagnostics
# ============================================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "  Trading HelloWorld Configuration")
message(STATUS "========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "========================================")
message(STATUS "")
