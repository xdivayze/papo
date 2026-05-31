add_executable(service_tests logger_test.cpp stack_allocater_test.cpp memory_manager_test.cpp pool_allocater_test.cpp pool_manager_test.cpp string_manager_test.cpp thread_pool_manager_test.cpp arena_allocator_test.cpp event_test.cpp time_manager_test.cpp mesh_test.cpp model_test.cpp asset_manager_test.cpp renderable_object_test.cpp filesystem_test.cpp)

target_link_libraries(service_tests
    PRIVATE
    services
    GTest::gtest_main
)

gtest_discover_tests(service_tests)
