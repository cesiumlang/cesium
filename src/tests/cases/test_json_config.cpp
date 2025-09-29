/**
@brief Tests for JSON configuration file loading and parsing functionality
*/
#include <filesystem>
#include <fstream>
#include <backend/core/json.h>
#include "../testfrmwk/simple_test.h"

// Test data setup
std::string test_config_path = "test_config.json";
std::string test_output_dir = "test_output";

void setupTestConfig() {
  // Create a test configuration file
  std::ofstream config_file(test_config_path);
  config_file << R"({
    "languages": {
      "cpp": {
        "library": "tree-sitter-cpp.so",
        "function": "tree_sitter_cpp",
        "extensions": [".cpp", ".hpp", ".cc", ".h", ".cxx"],
        "docstring_style": "/** */"
      },
      "python": {
        "library": "tree-sitter-python.so", 
        "function": "tree_sitter_python",
        "extensions": [".py"],
        "docstring_style": "\"\"\" \"\"\""
      }
    },
    "source_directories": ["src/", "include/"],
    "output_directory": "docs/extracted/",
    "exclude_patterns": ["**/test/**", "**/*_test.*"]
  })";
  config_file.close();
}

void teardownTestConfig() {
  std::filesystem::remove(test_config_path);
  std::filesystem::remove_all(test_output_dir);
}

/**
@brief Tests loading and parsing of a valid JSON configuration file

This test validates the requirement that the system must be able to load and parse
valid JSON configuration files containing language parser definitions, source directories,
and output settings. It ensures the JsonDoc class can handle real-world config structures.

Requirements tested:
- JsonDoc::fromFile() must successfully load existing files
- Loaded documents must report as valid
- Nested JSON objects must be accessible (languages.cpp.library)
- String values must be correctly extracted from nested structures
- Configuration values must match expected content exactly

Testing rationale: Configuration loading is critical for system operation - if configs
cannot be loaded or parsed correctly, the entire documentation generation system fails.
*/
void test_load_valid_config() {
  auto doc_opt = JsonDoc::fromFile(test_config_path);
  TEST_ASSERT_TRUE(doc_opt.has_value(), "load_valid_config_file");
  TEST_ASSERT_TRUE(doc_opt->isValid(), "doc_is_valid");
  
  auto doc = std::move(doc_opt.value());
  
  // Test accessing simple values - use intermediate variables to avoid chaining issues
  auto languages_proxy = doc[std::string("languages")];
  auto cpp_proxy = languages_proxy[std::string("cpp")];
  auto library_proxy = cpp_proxy[std::string("library")];
  
  std::string library_value = library_proxy;
  TEST_ASSERT_EQ(library_value, "tree-sitter-cpp.so", "library_value");
  
  auto docstring_proxy = cpp_proxy[std::string("docstring_style")];
  std::string docstring_value = docstring_proxy;
  TEST_ASSERT_EQ(docstring_value, "/** */", "docstring_style_value");
}

/**
@brief Tests graceful handling of nonexistent configuration files

This test validates the requirement that the system must handle missing configuration
files gracefully without crashing, returning an appropriate error indication that
can be checked by calling code.

Requirements tested:
- JsonDoc::fromFile() must return nullopt for nonexistent files
- No exceptions should be thrown for missing files
- System should fail gracefully rather than crash

Testing rationale: Robust error handling for missing config files prevents crashes
when users specify incorrect paths or when config files are accidentally deleted.
*/
void test_load_invalid_file() {
  auto doc_opt = JsonDoc::fromFile("nonexistent_config.json");
  TEST_ASSERT_FALSE(doc_opt.has_value(), "load_nonexistent_file");
}

void test_load_malformed_json() {
  std::string malformed_path = "malformed_config.json";
  std::ofstream malformed_file(malformed_path);
  malformed_file << R"({"invalid": json syntax})";
  malformed_file.close();
  
  auto doc_opt = JsonDoc::fromFile(malformed_path);
  TEST_ASSERT_FALSE(doc_opt.has_value(), "load_malformed_json");
  
  std::filesystem::remove(malformed_path);
}

void test_access_arrays() {
  auto doc_opt = JsonDoc::fromFile(test_config_path);
  if (!doc_opt.has_value()) {
    TEST_ASSERT_TRUE(false, "load_config_for_array_test");
    return;
  }
  
  auto doc = std::move(doc_opt.value());
  
  // Test source_directories array - use const version which returns JsonValue
  const auto& const_doc = doc;
  auto source_dirs = const_doc[std::string("source_directories")];
  TEST_ASSERT_TRUE(source_dirs.isArray(), "source_dirs_is_array");
  TEST_ASSERT_EQ(source_dirs.size(), 2, "source_dirs_array_size");
  
  auto first_dir = source_dirs[0];
  TEST_ASSERT_TRUE(first_dir.isString(), "first_dir_is_string");
  TEST_ASSERT_EQ(first_dir.asString(), "src/", "first_dir_value");
  
  // Test extensions array in cpp language  
  auto languages_val = const_doc[std::string("languages")];
  auto cpp_val = languages_val[std::string("cpp")];
  auto extensions_val = cpp_val[std::string("extensions")];
  TEST_ASSERT_TRUE(extensions_val.isArray(), "extensions_is_array");
  TEST_ASSERT_EQ(extensions_val.size(), 5, "ext_array_size");
  
  // Verify specific extensions
  TEST_ASSERT_EQ(extensions_val[0].asString(), ".cpp", "first_extension");
  TEST_ASSERT_EQ(extensions_val[1].asString(), ".hpp", "second_extension");
  TEST_ASSERT_EQ(extensions_val[4].asString(), ".cxx", "fifth_extension");
}

void test_access_missing_keys() {
  auto doc_opt = JsonDoc::fromFile(test_config_path);
  if (!doc_opt.has_value()) {
    TEST_ASSERT_TRUE(false, "load_config_for_missing_key_test");
    return;
  }
  
  auto doc = std::move(doc_opt.value());
  const auto& const_doc = doc;
  
  // Test accessing non-existent key
  auto missing_key = const_doc[std::string("nonexistent_key")];
  TEST_ASSERT_TRUE(missing_key.isNull(), "missing_key_is_null");
}

void test_multiple_languages() {
  auto doc_opt = JsonDoc::fromFile(test_config_path);
  if (!doc_opt.has_value()) {
    TEST_ASSERT_TRUE(false, "load_config_for_multi_lang_test");
    return;
  }
  
  auto doc = std::move(doc_opt.value());
  const auto& const_doc = doc;
  
  // Test cpp language
  auto languages_val = const_doc[std::string("languages")];
  auto cpp_val = languages_val[std::string("cpp")];
  auto cpp_function_val = cpp_val[std::string("function")];
  TEST_ASSERT_TRUE(cpp_function_val.isString(), "cpp_function_is_string");
  TEST_ASSERT_EQ(cpp_function_val.asString(), "tree_sitter_cpp", "cpp_function_name");
  
  // Test python language
  auto python_val = languages_val[std::string("python")];
  auto python_function_val = python_val[std::string("function")];
  TEST_ASSERT_TRUE(python_function_val.isString(), "python_function_is_string");
  TEST_ASSERT_EQ(python_function_val.asString(), "tree_sitter_python", "python_function_name");
  
  auto python_docstring_val = python_val[std::string("docstring_style")];
  TEST_ASSERT_TRUE(python_docstring_val.isString(), "python_docstring_style_is_string");
  TEST_ASSERT_EQ(python_docstring_val.asString(), "\"\"\" \"\"\"", "python_docstring_style");
}

void run_json_config_tests() {
  setupTestConfig();
  
  test_load_valid_config();
  test_load_invalid_file();
  test_load_malformed_json();
  test_access_arrays();
  test_access_missing_keys();
  test_multiple_languages();
  
  teardownTestConfig();
}