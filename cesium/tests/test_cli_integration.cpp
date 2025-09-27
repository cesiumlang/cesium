#include <filesystem>
#include <fstream>
#include "doc_cli.h"
#include "../../tests/simple_test.h"

std::string integration_test_dir = "integration_test_files";
std::string integration_output_dir = "integration_test_output";
std::string integration_config_file = "integration_test_config.json";

void setupIntegrationTest() {
  // Create test directories
  std::filesystem::create_directories(integration_test_dir + "/src");
  std::filesystem::create_directories(integration_test_dir + "/include");
  std::filesystem::create_directories(integration_output_dir);

  // Create test source files with documentation
  std::ofstream cpp_file(integration_test_dir + "/src/calculator.cpp");
  cpp_file << R"(
/**
 * Add two numbers together
 * @param a First number
 * @param b Second number
 * @return Sum of a and b
 */
int add(int a, int b) {
    return a + b;
}

/**
 * Multiply two numbers
 * @param x First number
 * @param y Second number
 * @return Product of x and y
 */
int multiply(int x, int y) {
    return x * y;
}
)";
  cpp_file.close();

  std::ofstream header_file(integration_test_dir + "/include/utils.h");
  header_file << R"(
/**
 * Utility class for mathematical operations
 *
 * This class provides common mathematical functions
 * that are used throughout the application.
 */
class MathUtils {
  public:
    /**
     * Calculate the square of a number
     * @param n The input number
     * @return The square of n
     */
    static int square(int n);
};
)";
  header_file.close();

  // Create test configuration file
  std::ofstream config_file(integration_config_file);
  config_file << R"({
    "languages": {
      "cpp": {
        "library": "tree-sitter-cpp.so",
        "function": "tree_sitter_cpp",
        "extensions": [".cpp", ".hpp", ".cc", ".h", ".cxx"],
        "javadoc_style": "/** */"
      }
    },
    "source_directories": [")" << integration_test_dir << R"(/src/", ")" << integration_test_dir << R"(/include/"],
    "output_directory": ")" << integration_output_dir << R"(/",
    "exclude_patterns": ["**/test/**", "**/*_test.*"]
  })";
  config_file.close();
}

void teardownIntegrationTest() {
  std::filesystem::remove_all(integration_test_dir);
  std::filesystem::remove_all(integration_output_dir);
  std::filesystem::remove(integration_config_file);
}

void test_cli_generate_command() {
  CesiumDocCLI cli;

  // Simulate command line arguments for generate command (as received by doc CLI)
  const char* argv[] = {"doc", "generate", "--config", integration_config_file.c_str()};
  int argc = 4;

  // Run the CLI command
  int result = cli.run(argc, const_cast<char**>(argv));

  TEST_ASSERT_EQ(result, 0, "cli_generate_returns_success");

  // Check that output files were created
  TEST_ASSERT_TRUE(std::filesystem::exists(integration_output_dir), "output_directory_exists");

  // Count generated markdown files
  int markdown_files = 0;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(integration_output_dir)) {
    if (entry.path().extension() == ".md") {
      markdown_files++;
    }
  }

  TEST_ASSERT_TRUE(markdown_files > 0, "markdown_files_generated");
}

void test_cli_list_parsers_command() {
  CesiumDocCLI cli;

  // Simulate command line arguments for list-parsers command (as received by doc CLI)
  const char* argv[] = {"doc", "list-parsers"};
  int argc = 2;

  // Run the CLI command (this will output to stdout)
  int result = cli.run(argc, const_cast<char**>(argv));

  // The command should succeed (exit code 0)
  TEST_ASSERT_EQ(result, 0, "cli_list_parsers_returns_success");
}

void test_cli_init_config_command() {
  std::string init_config_file = "test_init_config.json";

  CesiumDocCLI cli;

  // Simulate command line arguments for init-config command (as received by doc CLI)
  const char* argv[] = {"doc", "init-config", init_config_file.c_str()};
  int argc = 3;

  // Run the CLI command
  int result = cli.run(argc, const_cast<char**>(argv));

  TEST_ASSERT_EQ(result, 0, "cli_init_config_returns_success");
  TEST_ASSERT_TRUE(std::filesystem::exists(init_config_file), "init_config_file_created");

  // Verify the generated config file is valid JSON
  {
    std::ifstream config_stream(init_config_file);
    std::string config_content((std::istreambuf_iterator<char>(config_stream)), std::istreambuf_iterator<char>());

    // Basic JSON validation - should contain expected keys
    TEST_ASSERT_TRUE(config_content.find("languages") != std::string::npos, "init_config_has_languages");
    TEST_ASSERT_TRUE(config_content.find("source_directories") != std::string::npos, "init_config_has_source_dirs");
    TEST_ASSERT_TRUE(config_content.find("output_directory") != std::string::npos, "init_config_has_output_dir");
  } // Scope ensures file is closed before removal

  std::filesystem::remove(init_config_file);
}

void test_cli_invalid_command() {
  CesiumDocCLI cli;

  // Simulate invalid command (as received by doc CLI)
  const char* argv[] = {"doc", "invalid-command"};
  int argc = 2;

  // Run the CLI command - should return non-zero for invalid command
  int result = cli.run(argc, const_cast<char**>(argv));

  TEST_ASSERT_TRUE(result != 0, "cli_invalid_command_returns_error");
}

void test_cli_generate_with_missing_config() {
  CesiumDocCLI cli;

  // Simulate generate command with non-existent config file (as received by doc CLI)
  const char* argv[] = {"doc", "generate", "--config", "nonexistent_config.json"};
  int argc = 4;

  // Run the CLI command - should return non-zero for missing config
  int result = cli.run(argc, const_cast<char**>(argv));

  TEST_ASSERT_TRUE(result != 0, "cli_missing_config_returns_error");
}

void test_cli_generate_with_malformed_config() {
  std::string malformed_config = "malformed_integration_config.json";

  // Create malformed config file
  std::ofstream bad_config(malformed_config);
  bad_config << R"({"invalid": json, "syntax": here})";
  bad_config.close();

  CesiumDocCLI cli;

  // Simulate generate command with malformed config file (as received by doc CLI)
  const char* argv[] = {"doc", "generate", "--config", malformed_config.c_str()};
  int argc = 4;

  // Run the CLI command - should return non-zero for malformed config
  int result = cli.run(argc, const_cast<char**>(argv));

  TEST_ASSERT_TRUE(result != 0, "cli_malformed_config_returns_error");

  std::filesystem::remove(malformed_config);
}

void test_cli_no_arguments() {
  CesiumDocCLI cli;

  // Simulate running with no arguments (just "doc" as received by doc CLI)
  const char* argv[] = {"doc"};
  int argc = 1;

  // Run the CLI command - should show help/usage
  int result = cli.run(argc, const_cast<char**>(argv));

  // Depending on implementation, this might return 0 (showing help) or non-zero (error)
  // The important thing is that it doesn't crash
  TEST_ASSERT_TRUE(result >= 0, "cli_no_args_handled_gracefully");
}

void test_end_to_end_documentation_generation() {
  CesiumDocCLI cli;

  // Run full documentation generation (as received by doc CLI)
  const char* argv[] = {"doc", "generate", "--config", integration_config_file.c_str()};
  int argc = 4;

  int result = cli.run(argc, const_cast<char**>(argv));
  TEST_ASSERT_EQ(result, 0, "e2e_generation_success");

  // Verify specific files were generated with correct content
  bool found_add_function = false;
  bool found_multiply_function = false;
  bool found_math_utils_class = false;

  for (const auto& entry : std::filesystem::recursive_directory_iterator(integration_output_dir)) {
    if (entry.path().extension() == ".md") {
      std::ifstream file(entry.path());
      std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

      if (content.find("Add two numbers together") != std::string::npos) {
        found_add_function = true;
      }
      if (content.find("Multiply two numbers") != std::string::npos) {
        found_multiply_function = true;
      }
      if (content.find("Utility class for mathematical operations") != std::string::npos) {
        found_math_utils_class = true;
      }
    }
  }

  TEST_ASSERT_TRUE(found_add_function, "e2e_found_add_function_doc");
  TEST_ASSERT_TRUE(found_multiply_function, "e2e_found_multiply_function_doc");
  TEST_ASSERT_TRUE(found_math_utils_class, "e2e_found_class_doc");
}

void run_cli_integration_tests() {
  setupIntegrationTest();

  test_cli_generate_command();
  test_cli_list_parsers_command();
  test_cli_init_config_command();
  test_cli_invalid_command();
  test_cli_generate_with_missing_config();
  test_cli_generate_with_malformed_config();
  test_cli_no_arguments();
  test_end_to_end_documentation_generation();

  teardownIntegrationTest();
}
