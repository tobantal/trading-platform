# Broker Service CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# ============================================================================
# ИСХОДНИКИ
# ============================================================================
set(BROKER_SOURCES
    src/main.cpp
    src/domain/events/OrderCreatedEvent.cpp
    src/domain/events/OrderFilledEvent.cpp
    src/domain/events/OrderCancelledEvent.cpp
    src/domain/events/QuoteUpdatedEvent.cpp
)

# ============================================================================
# ИСПОЛНЯЕМЫЙ ФАЙЛ
# ============================================================================
add_executable(broker-service ${BROKER_SOURCES})

target_include_directories(broker-service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(broker-service PRIVATE
    microservice-core
    microservice-boost
    cache
    pqxx
    amqpcpp
    OpenSSL::SSL
    OpenSSL::Crypto
)

# ============================================================================
# ТЕСТЫ
# ============================================================================
if(BUILD_TESTS)
    file(GLOB_RECURSE BROKER_TEST_SOURCES
        CONFIGURE_DEPENDS
        tests/*.cpp
    )
    
    add_executable(broker-service-tests ${BROKER_TEST_SOURCES})
    
    target_include_directories(broker-service-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    target_link_libraries(broker-service-tests PRIVATE
        cache
        microservice-core
        microservice-boost
        GTest::gtest_main
        GTest::gmock
    )
    
    include(GoogleTest)
    gtest_discover_tests(broker-service-tests)
endif()
