add_executable(service_tests logger_test.cpp)

target_link_libraries(service_tests
    PRIVATE
    services
    GTest::gtest_main
)

gtest_discover_tests(service_tests)
