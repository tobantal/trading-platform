# Auth Service CMakeLists.txt
# Используется как подпроект родительского CMakeLists.txt

# ============================================
# AUTH SERVICE SOURCES
# ============================================
set(AUTH_SOURCES
    src/main.cpp
    src/AuthApp.cpp
)

# ============================================
# EXECUTABLE
# ============================================
add_executable(auth-service ${AUTH_SOURCES})

target_include_directories(auth-service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(auth-service PRIVATE
    microservice-core
    microservice-boost
    pqxx
)

# ============================================
# TESTS
# ============================================
if(BUILD_TESTS)
    # Собираем тестовые исходники
    set(AUTH_TEST_SOURCES
        tests/AuthServiceTest.cpp
        tests/AccountServiceTest.cpp
        tests/AuthEndpointTest.cpp
    )
    
    add_executable(auth-service-tests ${AUTH_TEST_SOURCES})
    
    target_include_directories(auth-service-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks
    )
    
    target_link_libraries(auth-service-tests PRIVATE
        microservice-core
        microservice-boost
        GTest::gtest_main
        GTest::gmock
    )
    
    include(GoogleTest)
    gtest_discover_tests(auth-service-tests)
endif()
