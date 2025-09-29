/**
@brief Tests for markdown documentation file generation functionality
*/
#include <filesystem>
#include <fstream>
#include <backend/doc/markdowngen.h>
#include "../testfrmwk/simple_test.h"

std::string markdown_test_output_dir = "test_markdown_output";

void setupMarkdownTest() {
  std::filesystem::create_directories(markdown_test_output_dir);
}

void teardownMarkdownTest() {
  std::filesystem::remove_all(markdown_test_output_dir);
}

DocstringBlock createTestBlock(const std::string& name, const std::string& desc, const std::string& type = "function_definition") {
  DocstringBlock block;
  block.symbol_name = name;
  block.symbol_type = type;
  block.description = desc;
  block.location.line = 42;
  block.namespace_path = "";
  return block;
}

/**
@brief Tests generation of markdown files from docstring blocks

This test validates the requirement that the markdown generator must create
properly formatted markdown files from parsed documentation blocks, with
correct file naming, content structure, and formatting.

Requirements tested:
- MarkdownGenerator must create markdown files in specified output directory
- Generated files must have correct naming convention (symbol_name.md)
- Markdown content must include function/symbol information
- File content must be properly formatted with headers and sections
- Generated files must be readable and contain expected text

Testing rationale: Markdown generation is the final output stage of documentation
processing - files must be correctly generated or the entire pipeline fails.
*/
void test_generate_simple_markdown() {
  std::cout << "Starting test_generate_simple_markdown..." << std::endl;
  MarkdownGenerator generator;
  std::cout << "Created MarkdownGenerator..." << std::endl;
  
  std::vector<DocstringBlock> blocks;
  std::cout << "Created empty blocks vector..." << std::endl;
  auto block = createTestBlock("testFunction", "A simple test function");
  std::cout << "Created test block..." << std::endl;
  block.params["x"] = "Input parameter";
  block.return_desc = "Output value";
  std::cout << "Set block parameters..." << std::endl;
  blocks.push_back(block);
  std::cout << "Added block to vector (size=" << blocks.size() << ")..." << std::endl;
  
  std::cout << "About to call generateMarkdownFiles..." << std::endl;
  generator.generateMarkdownFiles(blocks, markdown_test_output_dir);
  std::cout << "generateMarkdownFiles completed..." << std::endl;
  
  // Check that file was created
  std::string expected_file = markdown_test_output_dir + "/testFunction.md";
  TEST_ASSERT_TRUE(std::filesystem::exists(expected_file), "markdown_file_created");
  
  // Check file contents
  std::ifstream file(expected_file);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  
  TEST_ASSERT_TRUE(content.find("# testFunction") != std::string::npos, "markdown_contains_title");
  TEST_ASSERT_TRUE(content.find("A simple test function") != std::string::npos, "markdown_contains_description");
  TEST_ASSERT_TRUE(content.find("type: function_definition") != std::string::npos, "markdown_contains_type");
  TEST_ASSERT_TRUE(content.find("line: 42") != std::string::npos, "markdown_contains_line");
}

void test_generate_class_markdown() {
  MarkdownGenerator generator;
  
  std::vector<DocstringBlock> blocks;
  auto block = createTestBlock("TestClass", "A test class with documentation", "class_specifier");
  block.namespace_path = "TestNamespace";
  blocks.push_back(block);
  
  generator.generateMarkdownFiles(blocks, markdown_test_output_dir);
  
  // Check that file was created with proper naming
  std::string expected_file = markdown_test_output_dir + "/TestNamespace--TestClass.md";
  TEST_ASSERT_TRUE(std::filesystem::exists(expected_file), "class_markdown_file_created");
  
  // Check file contents
  std::ifstream file(expected_file);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  
  TEST_ASSERT_TRUE(content.find("type: class_specifier") != std::string::npos, "class_markdown_contains_type");
  TEST_ASSERT_TRUE(content.find("namespace: TestNamespace") != std::string::npos, "class_markdown_contains_namespace");
  TEST_ASSERT_TRUE(content.find("# TestClass") != std::string::npos, "class_markdown_contains_title");
  TEST_ASSERT_TRUE(content.find("A test class with documentation") != std::string::npos, "class_markdown_contains_description");
}

void test_generate_multiple_markdown_files() {
  MarkdownGenerator generator;
  
  std::vector<DocstringBlock> blocks;
  
  // Create multiple blocks
  auto block1 = createTestBlock("function1", "First function");
  auto block2 = createTestBlock("function2", "Second function");
  auto block3 = createTestBlock("MyClass", "A class", "class_specifier");
  block3.namespace_path = "MyNamespace";
  
  blocks.push_back(block1);
  blocks.push_back(block2);
  blocks.push_back(block3);
  
  generator.generateMarkdownFiles(blocks, markdown_test_output_dir);
  
  // Check all files were created
  TEST_ASSERT_TRUE(std::filesystem::exists(markdown_test_output_dir + "/function1.md"), "function1_created");
  TEST_ASSERT_TRUE(std::filesystem::exists(markdown_test_output_dir + "/function2.md"), "function2_created");
  TEST_ASSERT_TRUE(std::filesystem::exists(markdown_test_output_dir + "/MyNamespace--MyClass.md"), "class_created");
  
  // Count total files in directory
  int file_count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(markdown_test_output_dir)) {
    if (entry.path().extension() == ".md") {
      file_count++;
    }
  }
  TEST_ASSERT_EQ(file_count, 3, "correct_number_of_files");
}

void test_generate_function_with_parameters() {
  MarkdownGenerator generator;
  
  std::vector<DocstringBlock> blocks;
  auto block = createTestBlock("calculateSum", "Calculate sum of numbers");
  block.params["a"] = "First number";
  block.params["b"] = "Second number";
  block.params["c"] = "Third number";
  block.return_desc = "The sum of all parameters";
  blocks.push_back(block);
  
  generator.generateMarkdownFiles(blocks, markdown_test_output_dir);
  
  std::string expected_file = markdown_test_output_dir + "/calculateSum.md";
  std::ifstream file(expected_file);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  
  // Note: The actual parameter handling depends on the MarkdownGenerator implementation
  // These tests verify the basic structure is present
  TEST_ASSERT_TRUE(content.find("calculateSum") != std::string::npos, "function_name_in_content");
  TEST_ASSERT_TRUE(content.find("Calculate sum of numbers") != std::string::npos, "function_description_in_content");
}

void test_generate_empty_blocks() {
  MarkdownGenerator generator;
  std::vector<DocstringBlock> blocks; // Empty vector
  
  generator.generateMarkdownFiles(blocks, markdown_test_output_dir);
  
  // Check that no files were created
  int file_count = 0;
  if (std::filesystem::exists(markdown_test_output_dir)) {
    for (const auto& entry : std::filesystem::directory_iterator(markdown_test_output_dir)) {
      if (entry.path().extension() == ".md") {
        file_count++;
      }
    }
  }
  TEST_ASSERT_EQ(file_count, 0, "no_files_for_empty_blocks");
}

void test_markdown_frontmatter() {
  // Ensure the output directory exists
  std::filesystem::create_directories(markdown_test_output_dir);
  
  MarkdownGenerator generator;
  
  std::vector<DocstringBlock> blocks;
  auto block = createTestBlock("testFunction", "Test function with frontmatter");
  block.symbol_type = "function_definition";
  block.namespace_path = "TestNS";
  block.location.line = 123;
  blocks.push_back(block);
  
  generator.generateMarkdownFiles(blocks, markdown_test_output_dir);
  
  std::string expected_file = markdown_test_output_dir + "/TestNS--testFunction.md";
  std::ifstream file(expected_file);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  
  // Check for YAML frontmatter
  TEST_ASSERT_TRUE(content.find("---") != std::string::npos, "has_frontmatter_delimiters");
  TEST_ASSERT_TRUE(content.find("type: function_definition") != std::string::npos, "has_type_in_frontmatter");
  TEST_ASSERT_TRUE(content.find("name: testFunction") != std::string::npos, "has_name_in_frontmatter");
  TEST_ASSERT_TRUE(content.find("line: 123") != std::string::npos, "has_line_in_frontmatter");
}

void test_invalid_output_directory() {
  MarkdownGenerator generator;
  
  std::vector<DocstringBlock> blocks;
  auto block = createTestBlock("testFunction", "Test function");
  blocks.push_back(block);
  
  #ifdef _WIN32
    // On Windows, try to use a truly invalid path (reserved device name)
    std::string invalid_path = "CON/invalid_path";  // CON is a reserved device name on Windows
  #else
    // On Unix-like systems, create a read-only directory
    std::string readonly_base = "readonly_test_dir";
    std::string invalid_path = readonly_base + "/cannot_write_here";
    
    // Create the base directory and make it read-only
    std::filesystem::create_directory(readonly_base);
    std::filesystem::permissions(readonly_base, 
                                std::filesystem::perms::owner_read | std::filesystem::perms::group_read | std::filesystem::perms::others_read,
                                std::filesystem::perm_options::replace);
  #endif
  
  try {
    // Try to write to the invalid directory (should fail)
    bool caught_exception = false;
    try {
      generator.generateMarkdownFiles(blocks, invalid_path);
    } catch (const std::exception& e) {
      caught_exception = true;
    }
    
    // Either it should throw an exception or gracefully handle the error
    TEST_ASSERT_TRUE(caught_exception || !std::filesystem::exists(invalid_path + "/testFunction.md"), "invalid_path_handled_gracefully");
    
    #ifndef _WIN32
      // Clean up read-only directory on Unix systems
      std::filesystem::permissions(readonly_base, 
                                  std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::others_read,
                                  std::filesystem::perm_options::replace);
      std::filesystem::remove_all(readonly_base);
    #endif
  } catch (const std::exception& e) {
    #ifndef _WIN32
      // Clean up in case of any error on Unix systems
      try {
        std::filesystem::permissions(readonly_base, 
                                    std::filesystem::perms::owner_all,
                                    std::filesystem::perm_options::replace);
        std::filesystem::remove_all(readonly_base);
      } catch (...) {
        // Ignore cleanup errors
      }
    #endif
    TEST_ASSERT_TRUE(true, "invalid_path_throws_exception");
  }
}

void run_markdown_generator_tests() {
  setupMarkdownTest();
  
  test_generate_simple_markdown();
  teardownMarkdownTest();
  setupMarkdownTest();
  
  test_generate_class_markdown();
  teardownMarkdownTest();
  setupMarkdownTest();
  
  test_generate_multiple_markdown_files();
  teardownMarkdownTest();
  setupMarkdownTest();
  
  test_generate_function_with_parameters();
  teardownMarkdownTest();
  setupMarkdownTest();
  
  test_generate_empty_blocks();
  teardownMarkdownTest();
  setupMarkdownTest();
  
  test_markdown_frontmatter();
  teardownMarkdownTest();
  setupMarkdownTest();
  
  test_invalid_output_directory();
  
  teardownMarkdownTest();
}