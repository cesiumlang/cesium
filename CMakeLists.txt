cmake_minimum_required(VERSION 3.22)
project(cesium VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set C++ standard to latest fully supported by Clang++ (C++20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

## configure CMake module search paths that depend on the project
## proj src dir is location of this file
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# # Cache FetchContent downloads outside build directory
# set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/../.depcache" CACHE PATH "FetchContent cache directory")

include(CompileCommands)
include(DefaultBuildType)
include(DefaultInstallPath)
include(ForceUnicode)
include(HideVSCodeVars)
include(ConfigFetchContent)

# include dependencies
include(qt)
include(yyjson)
include(tree-sitter-core)
include(tree-sitter-cpp)

# Add cesium subdirectory
add_subdirectory(cesium)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
