// Test what Tree-sitter parses for operator=
#include <iostream>
class Test {
public:
  Test& operator=(const Test& other) {
    return *this;
  }
  Test& operator=(Test&& other) noexcept {
    return *this;
  }
};

// Global operator function
int operator+(int a, int b) {
  return a + b;
}