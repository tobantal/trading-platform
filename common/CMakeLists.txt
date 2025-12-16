project(commonlib VERSION 1.0.1 LANGUAGES CXX)

# Собираем статическую библиотеку с исходниками
file(GLOB_RECURSE COMMON_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(commonlib STATIC ${COMMON_SOURCES})

# Делаем include/ публичным для подпроектов
target_include_directories(commonlib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Тесты для common
enable_testing()
file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
add_executable(common_tests ${TEST_SOURCES})
target_link_libraries(common_tests
  PRIVATE
    commonlib
    GTest::gtest_main
    GTest::gmock_main
)
include(GoogleTest)
gtest_discover_tests(common_tests)
