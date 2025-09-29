/**
@brief Test runner for executing all test suites
*/
#include "simple_test.h"
#include <backend/core/debug.h>

// Include all test functions - we'll need to declare these
void run_json_config_tests();
void run_docstring_parser_tests();  
void run_markdown_generator_tests();
void run_cli_integration_tests();

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
  
  // Print final summary and return appropriate exit code
  return SimpleTest::print_summary();
}