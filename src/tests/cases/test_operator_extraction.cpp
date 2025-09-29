/**
@brief Tests for C++ operator function name extraction from AST nodes
*/
#include <backend/doc/cpp/ast_extractor.h>
#include "../testfrmwk/simple_test.h"

/**
@brief Tests extraction of qualified operator function names (Class::operator=)

This test validates the requirement that the AST extractor must correctly identify
and extract C++ operator function names from qualified declarator text, preserving
the class qualification and operator symbol.

Requirements tested:
- extractFunctionNameFromText() must handle qualified operator names
- Class qualification must be preserved (JsonDoc::)
- Operator symbol must be correctly identified (operator=)
- Function parameters and qualifiers must be stripped

Testing rationale: C++ operator overloading is common in modern C++, and documentation
systems must handle these special function names correctly for complete API coverage.
*/
void test_qualified_operator_extraction() {
  ASTExtractor extractor;
  
  std::string test_input = "JsonDoc::operator=(JsonDoc&& other) noexcept";
  std::string result = extractor.extractFunctionNameFromText(test_input);
  
  TEST_ASSERT_EQ(result, "JsonDoc::operator=", "qualified_operator_assignment");
}

/**
@brief Tests extraction of unqualified operator function names (operator=)

This test validates the requirement that the AST extractor must correctly identify
operator functions even when not fully qualified, handling the special operator
keyword and symbol correctly.

Requirements tested:
- extractFunctionNameFromText() must handle unqualified operators
- Operator keyword and symbol must be preserved
- Function signature must be stripped properly

Testing rationale: Not all operator declarations include class qualification,
so the extractor must handle both forms consistently.
*/
void test_unqualified_operator_extraction() {
  ASTExtractor extractor;
  
  std::string test_input = "operator=(JsonDoc&& other) noexcept";
  std::string result = extractor.extractFunctionNameFromText(test_input);
  
  TEST_ASSERT_EQ(result, "operator=", "unqualified_operator_assignment");
}

/**
@brief Tests extraction of subscript operator function names (operator[])

This test validates the requirement that the AST extractor must handle complex
operator symbols including bracket operators, which are commonly used in
container and wrapper classes.

Requirements tested:
- extractFunctionNameFromText() must handle bracket operators
- Complex operator symbols ([][]) must be preserved
- Const qualifiers in function signature must be handled

Testing rationale: Subscript operators are fundamental to C++ container APIs
and must be documented correctly for usable API reference.
*/
void test_subscript_operator_extraction() {
  ASTExtractor extractor;
  
  std::string test_input = "JsonValue::operator[](const std::string& key) const";
  std::string result = extractor.extractFunctionNameFromText(test_input);
  
  TEST_ASSERT_EQ(result, "JsonValue::operator[]", "subscript_operator");
}

void test_unqualified_subscript_operator() {
  ASTExtractor extractor;
  
  std::string test_input = "operator[](const std::string& key) const";
  std::string result = extractor.extractFunctionNameFromText(test_input);
  
  TEST_ASSERT_EQ(result, "operator[]", "unqualified_subscript_operator");
}

void test_simple_function_extraction() {
  ASTExtractor extractor;
  
  std::string test_input = "someFunction(int a, int b)";
  std::string result = extractor.extractFunctionNameFromText(test_input);
  
  TEST_ASSERT_EQ(result, "someFunction", "simple_function");
}

void test_destructor_extraction() {
  ASTExtractor extractor;
  
  std::string test_input = "~JsonDoc()";
  std::string result = extractor.extractFunctionNameFromText(test_input);
  
  TEST_ASSERT_EQ(result, "~JsonDoc", "destructor");
}

void run_operator_extraction_tests() {
  test_qualified_operator_extraction();
  test_unqualified_operator_extraction();
  test_subscript_operator_extraction();
  test_unqualified_subscript_operator();
  test_simple_function_extraction();
  test_destructor_extraction();
}