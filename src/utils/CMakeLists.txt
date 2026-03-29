add_library(utils
    exception_handler.cpp
    )

target_include_directories(utils PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
