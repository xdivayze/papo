add_library(logger logger.cpp)

target_include_directories(logger PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
