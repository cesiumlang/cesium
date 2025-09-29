/**
@brief Main entry point for running all Cesium unit tests
*/
#include "testfrmwk/simple_test.h"
#include <backend/core/debug.h>
#include "cases.h"

int main() {
  debug::suppressErrorDialogs();

  std::cout << "Starting Cesium Documentation Generation Tests" << std::endl;

  SimpleTest::reset();

  // Run all test suites
  RUN_TEST_SUITE("JSON Configuration Tests", run_json_config_tests);
  std::cout << "*** JSON tests completed successfully ***" << std::endl;

  RUN_TEST_SUITE("Docstring Parser Tests", run_docstring_parser_tests);
  std::cout << "*** Docstring parser tests completed successfully ***" << std::endl;

  RUN_TEST_SUITE("Markdown Generator Tests", run_markdown_generator_tests);
  std::cout << "*** Markdown generator tests completed successfully ***" << std::endl;

  RUN_TEST_SUITE("CLI Integration Tests", run_cli_integration_tests);
  std::cout << "*** CLI integration tests completed successfully ***" << std::endl;

  RUN_TEST_SUITE("Operator Extraction Tests", run_operator_extraction_tests);
  std::cout << "*** Operator extraction tests completed successfully ***" << std::endl;

  RUN_TEST_SUITE("Dynamic Library Platform Tests", run_dynlib_platform_tests);
  std::cout << "*** Dynamic library platform tests completed successfully ***" << std::endl;

  // Print final summary and return appropriate exit code
  return SimpleTest::print_summary();
}
