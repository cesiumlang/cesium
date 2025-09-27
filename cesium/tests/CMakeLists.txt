# Cesium test source files
set(CESIUM_TEST_SOURCES
  test_json_config.cpp
  test_javadoc_parser.cpp  
  test_markdown_generator.cpp
  test_cli_integration.cpp
)

# Add these sources to the parent scope so the main test executable can use them
set(CESIUM_TEST_SOURCES ${CESIUM_TEST_SOURCES} PARENT_SCOPE)