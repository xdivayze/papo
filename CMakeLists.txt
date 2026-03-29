cmake_minimum_required(VERSION 3.16)
project(papo)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src/services)
add_subdirectory(src/utils)

enable_testing()
add_subdirectory(tests)
