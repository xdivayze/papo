cmake_minimum_required(VERSION 3.16)
project(papo)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src/services)
add_subdirectory(src/utils)

enable_testing()
add_subdirectory(tests)

add_subdirectory(external/glm EXCLUDE_FROM_ALL)
target_link_libraries(services PUBLIC glm::glm)

# disable GLFW extras
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw EXCLUDE_FROM_ALL)

# GLAD v2: provides glad_add_library() macro
add_subdirectory(external/glad/cmake EXCLUDE_FROM_ALL)
glad_add_library(glad STATIC API gl:core=4.6)

target_link_libraries(services PUBLIC glfw glad)

# Assimp — only build what we need
set(ASSIMP_BUILD_TESTS              OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL                  OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS       OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER       ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER      ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER       ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB               ON  CACHE BOOL "" FORCE)
add_subdirectory(external/assimp EXCLUDE_FROM_ALL)
target_link_libraries(services PUBLIC assimp)
