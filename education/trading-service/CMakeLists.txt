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
# ============================================================================
option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    enable_testing()
    
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    
    set(TRADING_TEST_SOURCES
        tests/adapters/CachedBrokerGatewayTest.cpp
        tests/adapters/OrderHandlerTest.cpp
        tests/adapters/HttpBrokerGatewayTest.cpp
        tests/application/OrderServiceTest.cpp
        tests/application/MarketServiceTest.cpp
        tests/application/PortfolioServiceTest.cpp
        tests/endpoints/MarketHandlerTest.cpp
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

# ============================================================================
# Install
# ============================================================================
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
