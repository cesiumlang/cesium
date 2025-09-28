# Add cesium tests subdirectory to get test source files
add_subdirectory(../cesium/tests cesium_tests_build)

# Create test executable for documentation generation functionality
add_executable(cesium_tests
  test_runner.cpp
  simple_test.cpp
)

# Add cesium test sources to the executable
foreach(test_source ${CESIUM_TEST_SOURCES})
  target_sources(cesium_tests PRIVATE ../cesium/tests/${test_source})
endforeach()

# Set properties for the test executable
set_target_properties(cesium_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Link against the same libraries as the main cesium executable
target_link_libraries(cesium_tests
  PRIVATE
    ${CMAKE_DL_LIBS}
    yyjson
    tree-sitter-core
    tree-sitter-cpp
    utf8
)

# Add include directories so tests can find headers
target_include_directories(cesium_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/cesium/include
  ${CMAKE_SOURCE_DIR}/tests
)

# We need to link against the cesium object files to test internal components
# Get all the source files from the cesium target (excluding main.cpp)
get_target_property(CESIUM_SOURCES cesium SOURCES)
list(FILTER CESIUM_SOURCES EXCLUDE REGEX "main\\.cpp$")

target_sources(cesium_tests PRIVATE ${CESIUM_SOURCES})

# Compiler warnings (same as main project)
target_compile_options(cesium_tests PRIVATE -Wall -Wextra -Wpedantic)

# Platform-specific settings
if(WIN32)
  target_compile_definitions(cesium_tests PRIVATE
    _WIN32_WINNT=0x0601
    WIN32_LEAN_AND_MEAN
  )
endif()

# Clang-specific flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(cesium_tests PRIVATE -Wno-unused-parameter)
endif()

# Add the test to CTest
add_test(NAME CesiumDocumentationTests COMMAND cesium_tests)

# Create tmp directory for test artifacts
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tmp)

# Set test properties
set_tests_properties(CesiumDocumentationTests PROPERTIES
  TIMEOUT 60
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tmp
)
