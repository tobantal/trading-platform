cmake_minimum_required(VERSION 3.16)
project(trading-mvp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================
# ОПЦИИ
# ============================================
option(BUILD_TESTS "Build tests" OFF)

# ============================================
# ЗАВИСИМОСТИ: FetchContent
# ============================================
include(FetchContent)

# cpp-http-server (включает Boost.Beast, Boost.DI, nlohmann/json)
FetchContent_Declare(
    cpp-http-server
    GIT_REPOSITORY https://github.com/tobantal/cpp-http-server.git
    GIT_TAG        v0.0.5
)

# cpp-cache (LRU cache)
FetchContent_Declare(
    cpp-cache
    GIT_REPOSITORY https://github.com/tobantal/cpp-cache.git
    GIT_TAG        v0.0.1
)

FetchContent_MakeAvailable(cpp-http-server cpp-cache)

# ============================================
# ИСХОДНИКИ
# ============================================
file(GLOB_RECURSE MVP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# ============================================
# EXECUTABLE
# ============================================
add_executable(${PROJECT_NAME} ${MVP_SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/domain
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ports/input
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ports/output
        ${CMAKE_CURRENT_SOURCE_DIR}/include/application
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adapters/primary
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adapters/secondary
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adapters/secondary/broker
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adapters/secondary/persistence
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adapters/secondary/auth
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adapters/secondary/cache
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adapters/secondary/events
        ${CMAKE_CURRENT_SOURCE_DIR}/include/strategies
)

target_link_libraries(trading-mvp
    PRIVATE
        microservice-core
        microservice-boost
        cpp_cache
        commonlib
)

# ============================================
# ТЕСТЫ (опционально)
# ============================================
if(BUILD_TESTS)
    enable_testing()
    # TODO: добавить тесты
endif()

# ============================================
# INSTALL
# ============================================
install(TARGETS ${PROJECT_NAME} DESTINATION bin)