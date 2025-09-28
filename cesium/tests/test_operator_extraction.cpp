#include "doc/cpp/ast_extractor.h"
#include "../../tests/simple_test.h"

void test_qualified_operator_extraction() {
  ASTExtractor extractor;
  
  std::string test_input = "JsonDoc::operator=(JsonDoc&& other) noexcept";
  std::string result = extractor.extractFunctionNameFromText(test_input);
  
  TEST_ASSERT_EQ(result, "JsonDoc::operator=", "qualified_operator_assignment");
}

void test_unqualified_operator_extraction() {
  ASTExtractor extractor;
  
  std::string test_input = "operator=(JsonDoc&& other) noexcept";
  std::string result = extractor.extractFunctionNameFromText(test_input);
  
  TEST_ASSERT_EQ(result, "operator=", "unqualified_operator_assignment");
}

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

int main() {
  test_qualified_operator_extraction();
  test_unqualified_operator_extraction();
  test_subscript_operator_extraction();
  test_unqualified_subscript_operator();
  test_simple_function_extraction();
  test_destructor_extraction();
  
  return 0;
}