cmake_minimum_required(VERSION 3.16)
project(trading-service VERSION 1.0.0 LANGUAGES CXX)

message(STATUS "")
message(STATUS "========================================")
message(STATUS "  Trading Service ${PROJECT_VERSION}")
message(STATUS "========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "========================================")
message(STATUS "")

# ============================================================================
# Executable
# ============================================================================

add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${boost_di_SOURCE_DIR}/include
    ${cpp_http_server_SOURCE_DIR}/microservice-core/include
    ${cpp_http_server_SOURCE_DIR}/microservice-boost/include
    ${cpp_cache_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    microservice-core
    microservice-boost
    pqxx
    amqpcpp
    cache
    OpenSSL::SSL
    OpenSSL::Crypto
)

# ============================================================================
# Tests (optional)
# GoogleTest уже загружен в родительском education/CMakeLists.txt
# НЕ дублируем FetchContent и enable_testing() здесь!
# ============================================================================

if(BUILD_TESTS)
    # НЕ вызываем enable_testing() - уже вызван в root
    # НЕ загружаем GoogleTest - уже загружен в education/CMakeLists.txt
    
    file(GLOB_RECURSE TRADING_TEST_SOURCES
        CONFIGURE_DEPENDS
        tests/*.cpp
    )
    
    add_executable(trading-service-tests ${TRADING_TEST_SOURCES})
    
    target_include_directories(trading-service-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${boost_di_SOURCE_DIR}/include
        ${cpp_http_server_SOURCE_DIR}/microservice-core/include
        ${cpp_http_server_SOURCE_DIR}/microservice-boost/include
        ${cpp_cache_SOURCE_DIR}/include
    )
    
    target_link_libraries(trading-service-tests PRIVATE
        microservice-core
        microservice-boost
        cache
        GTest::gtest_main
        GTest::gmock
        pthread
    )
    
    include(GoogleTest)
    gtest_discover_tests(trading-service-tests)
endif()