# ROOT CMakeLists.txt Trading Platform
cmake_minimum_required(VERSION 3.16)
project(trading-mvp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================
# ОПЦИИ
# ============================================
option(BUILD_TESTS "Build tests" ON) # OFF

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
    GIT_TAG        v1.0.4
)

FetchContent_MakeAvailable(cpp-http-server cpp-cache)

# Найти OpenSSL (ДО FetchContent amqpcpp)
find_package(OpenSSL REQUIRED)

# AMQP-CPP для RabbitMQ
FetchContent_Declare(
    amqpcpp
    GIT_REPOSITORY https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git
    GIT_TAG v4.3.26
)
# КРИТИЧНО: включить TCP поддержку
set(AMQP-CPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(AMQP-CPP_LINUX_TCP ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(amqpcpp)

# Сначала получаем libpq (PostgreSQL C library)
find_package(PostgreSQL REQUIRED)

# Получаем libpqxx
FetchContent_Declare(
    libpqxx
    GIT_REPOSITORY https://github.com/jtv/libpqxx.git
    GIT_TAG 7.9.2
)

# Опции для libpqxx (отключаем ненужное)
set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
set(BUILD_DOC OFF CACHE BOOL "" FORCE)
set(BUILD_TEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libpqxx)

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
        cache
        commonlib
        pqxx
        amqpcpp
        OpenSSL::SSL
        OpenSSL::Crypto
)

# ============================================
# ТЕСТЫ
# ============================================
if(BUILD_TESTS)
    enable_testing()
    
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    
    # Тесты адаптеров
    file(GLOB_RECURSE TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/adapters/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/endpoints/*.cpp"
    )
    
    add_executable(mvp_tests ${TEST_SOURCES} ${MVP_SOURCES})
    
    # Убираем main.cpp из тестов (там свой main от gtest)
    get_target_property(TEST_SRCS mvp_tests SOURCES)
    list(FILTER TEST_SRCS EXCLUDE REGEX ".*src/main\\.cpp$")
    set_target_properties(mvp_tests PROPERTIES SOURCES "${TEST_SRCS}")
    
    target_include_directories(mvp_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/include/domain
            ${CMAKE_CURRENT_SOURCE_DIR}/include/domain/enums
            ${CMAKE_CURRENT_SOURCE_DIR}/include/domain/events
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

    target_link_libraries(mvp_tests
    PRIVATE
        microservice-core
        microservice-boost
        cache
        commonlib
        amqpcpp
        OpenSSL::SSL
        OpenSSL::Crypto
        GTest::gtest_main
        GTest::gmock_main
)
    
    include(GoogleTest)
    gtest_discover_tests(mvp_tests)
endif()

# ============================================
# INSTALL
# ============================================
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
