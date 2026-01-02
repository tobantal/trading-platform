# Broker Service CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# ============================================================================
# ИСХОДНИКИ
# ============================================================================
set(BROKER_SOURCES
    src/main.cpp
    src/BrokerApp.cpp
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
    pqxx
    amqpcpp
    OpenSSL::SSL
    OpenSSL::Crypto
)

# ============================================================================
# ТЕСТЫ
# ============================================================================
if(BUILD_TESTS)
    set(BROKER_TEST_SOURCES
        tests/adapters/MarketScenarioTest.cpp
        tests/adapters/PriceSimulatorTest.cpp
        tests/adapters/OrderProcessorTest.cpp
        tests/adapters/BackgroundTickerTest.cpp
        tests/adapters/EnhancedFakeBrokerTest.cpp
    )
    
    add_executable(broker-service-tests ${BROKER_TEST_SOURCES})
    
    target_include_directories(broker-service-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    target_link_libraries(broker-service-tests PRIVATE
        microservice-core
        microservice-boost
        GTest::gtest_main
        GTest::gmock
    )
    
    include(GoogleTest)
    gtest_discover_tests(broker-service-tests)
endif()
