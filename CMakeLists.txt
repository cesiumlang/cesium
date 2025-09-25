cmake_minimum_required(VERSION 3.22)
project(cesium VERSION 0.1.0 LANGUAGES C CXX)

# Set C standard to C17
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set C++ standard to latest fully supported by Clang++ (C++20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# include(FetchContent)
# set(FETCHCONTENT_QUIET FALSE CACHE BOOL "Suppress output from FetchContent" FORCE)

# Define the executable with modular source files
add_executable(cesium src/main.cpp)

# Compiler warnings (all platforms)
target_compile_options(cesium PRIVATE -Wall -Wextra -Wpedantic)

# Platform-specific settings
if(WIN32)
  target_compile_definitions(cesium PRIVATE
    _WIN32_WINNT=0x0601
    WIN32_LEAN_AND_MEAN
  )
  # no gui
  set_target_properties(cesium PROPERTIES WIN32_EXECUTABLE FALSE)
  # Force console subsystem for MSVC or MinGW, but not for Clang/lld-link
  if(MSVC OR MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--subsystem,console")
  endif()
  # Optionally add Windows-specific libraries here if needed
elseif(APPLE)
  # macOS-specific settings (if any)
elseif(UNIX)
  # Linux-specific settings (if any)
endif()

# Clang-specific flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(cesium PRIVATE -Wno-unused-parameter)
endif()

# Set output directory for all platforms
set_target_properties(cesium PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation rules
install(TARGETS cesium RUNTIME DESTINATION bin)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
