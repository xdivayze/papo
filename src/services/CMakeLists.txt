add_library(services
    logger.cpp
    services.cpp
    memory/stack_allocater.cpp
    )

target_include_directories(services PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
