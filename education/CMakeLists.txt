# ROOT CMakeLists.txt - Trading Platform Microservices
cmake_minimum_required(VERSION 3.16)
project(trading-platform VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================
# OPTIONS
# ============================================
option(BUILD_TESTS "Build tests" ON)
option(BUILD_AUTH_SERVICE "Build Auth Service" ON)
option(BUILD_TRADING_SERVICE "Build Trading Service" ON)
option(BUILD_BROKER_SERVICE "Build Broker Service" ON)

# ============================================
# DEPENDENCIES: FetchContent
# ============================================
include(FetchContent)

message(STATUS "========================================")
message(STATUS "Fetching dependencies...")
message(STATUS "========================================")

# cpp-http-server (включает Boost.Beast, Boost.DI, nlohmann/json)
message(STATUS "Fetching cpp-http-server...")
FetchContent_Declare(
    cpp-http-server
    GIT_REPOSITORY https://github.com/tobantal/cpp-http-server.git
    GIT_TAG v0.0.5
)

# cpp-cache (LRU cache с TTL)
message(STATUS "Fetching cpp-cache...")
FetchContent_Declare(
    cpp-cache
    GIT_REPOSITORY https://github.com/tobantal/cpp-cache.git
    GIT_TAG v1.0.4
)

FetchContent_MakeAvailable(cpp-http-server cpp-cache)

# PostgreSQL (libpq + libpqxx)
find_package(PostgreSQL REQUIRED)

message(STATUS "Fetching libpqxx...")
FetchContent_Declare(
    libpqxx
    GIT_REPOSITORY https://github.com/jtv/libpqxx.git
    GIT_TAG 7.9.2
)
set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
set(BUILD_DOC OFF CACHE BOOL "" FORCE)
set(BUILD_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libpqxx)

# ============================================
# GOOGLE TEST (for testing)
# ============================================
if(BUILD_TESTS)
    message(STATUS "Fetching GoogleTest...")
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

# ============================================
# MICROSERVICES
# ============================================
message(STATUS "========================================")
message(STATUS "Building microservices...")
message(STATUS "========================================")

if(BUILD_AUTH_SERVICE)
    message(STATUS "Adding auth-service...")
    add_subdirectory(auth-service)
endif()

if(BUILD_TRADING_SERVICE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/trading-service/CMakeLists.txt")
    message(STATUS "Adding trading-service...")
    add_subdirectory(trading-service)
endif()

if(BUILD_BROKER_SERVICE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/broker-service/CMakeLists.txt")
    message(STATUS "Adding broker-service...")
    add_subdirectory(broker-service)
endif()

# ============================================
# SUMMARY
# ============================================
message(STATUS "========================================")
message(STATUS "Build configuration:")
message(STATUS "  BUILD_TESTS: ${BUILD_TESTS}")
message(STATUS "  BUILD_AUTH_SERVICE: ${BUILD_AUTH_SERVICE}")
message(STATUS "  BUILD_TRADING_SERVICE: ${BUILD_TRADING_SERVICE}")
message(STATUS "  BUILD_BROKER_SERVICE: ${BUILD_BROKER_SERVICE}")
message(STATUS "========================================")
