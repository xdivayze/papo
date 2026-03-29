add_library(services logger.cpp services.cpp)

target_include_directories(services PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
