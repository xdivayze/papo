add_library(services
    logger.cpp
    services.cpp
    memory/stack_allocater.cpp
    memory/memory_manager.cpp
    string_manager.cpp
    async/thread_pool_manager.cpp
    )

target_include_directories(services PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
