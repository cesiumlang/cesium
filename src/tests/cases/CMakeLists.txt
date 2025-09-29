target_include_directories(cesium-tests PRIVATE .)
target_sources(cesium-tests PRIVATE
  test_json_config.cpp
  test_docstring_parser.cpp
  test_markdown_generator.cpp
  test_cli_integration.cpp
  test_operator_extraction.cpp
  test_dynlib_platform.cpp
)
