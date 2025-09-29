# Create test executable for documentation generation functionality
add_executable(cesium-tests)
ExeTgtDefaults(cesium-tests)

add_test(NAME CesiumTests COMMAND cesium-tests)
set_tests_properties(CesiumTests PROPERTIES
  TIMEOUT 60
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tmp
)

# Create tmp directory for test artifacts
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tmp)

target_link_libraries(cesium-tests cesium-backend yyjson tree-sitter-core tree-sitter-cpp utf8)
target_include_directories(cesium-tests PRIVATE . cases/)
target_sources(cesium-tests PRIVATE main.cpp)
add_subdirectory(testfrmwk)
add_subdirectory(cases)
