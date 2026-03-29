add_executable(util_tests exception_test.cpp)

target_link_libraries(util_tests
    PRIVATE
    utils
    services
    GTest::gtest_main
)

gtest_discover_tests(util_tests)
