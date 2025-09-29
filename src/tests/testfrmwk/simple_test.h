/**
@brief Simple lightweight testing framework for Cesium
*/
#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <vector>
#include <sstream>

class SimpleTest {
  private:
    static int tests_run_;
    static int tests_failed_;
    static std::vector<std::string> failed_tests_;
    
  public:
    static void assert_true(bool condition, const std::string& test_name, const std::string& message = "") {
      tests_run_++;
      if (!condition) {
        tests_failed_++;
        std::stringstream ss;
        ss << "FAIL: " << test_name;
        if (!message.empty()) {
          ss << " - " << message;
        }
        failed_tests_.push_back(ss.str());
        std::cerr << ss.str() << std::endl;
      } else {
        std::cout << "PASS: " << test_name << std::endl;
      }
    }
    
    static void assert_false(bool condition, const std::string& test_name, const std::string& message = "") {
      assert_true(!condition, test_name, message);
    }
    
    static void assert_equals(const std::string& expected, const std::string& actual, 
                             const std::string& test_name, const std::string& message = "") {
      tests_run_++;
      if (expected != actual) {
        tests_failed_++;
        std::stringstream ss;
        ss << "FAIL: " << test_name << " - Expected: '" << expected << "', Got: '" << actual << "'";
        if (!message.empty()) {
          ss << " (" << message << ")";
        }
        failed_tests_.push_back(ss.str());
        std::cerr << ss.str() << std::endl;
      } else {
        std::cout << "PASS: " << test_name << std::endl;
      }
    }
    
    static void assert_equals(int expected, int actual, const std::string& test_name, const std::string& message = "") {
      tests_run_++;
      if (expected != actual) {
        tests_failed_++;
        std::stringstream ss;
        ss << "FAIL: " << test_name << " - Expected: " << expected << ", Got: " << actual;
        if (!message.empty()) {
          ss << " (" << message << ")";
        }
        failed_tests_.push_back(ss.str());
        std::cerr << ss.str() << std::endl;
      } else {
        std::cout << "PASS: " << test_name << std::endl;
      }
    }
    
    template<typename T>
    static void assert_equals(T expected, T actual, const std::string& test_name, const std::string& message = "") {
      tests_run_++;
      if (expected != actual) {
        tests_failed_++;
        std::stringstream ss;
        ss << "FAIL: " << test_name << " - Expected: " << expected << ", Got: " << actual;
        if (!message.empty()) {
          ss << " (" << message << ")";
        }
        failed_tests_.push_back(ss.str());
        std::cerr << ss.str() << std::endl;
      } else {
        std::cout << "PASS: " << test_name << std::endl;
      }
    }
    
    static void run_test_suite(const std::string& suite_name, std::function<void()> test_function) {
      std::cout << "\n=== Running " << suite_name << " ===" << std::endl;
      int initial_tests = tests_run_;
      int initial_failures = tests_failed_;
      
      try {
        test_function();
      } catch (const std::exception& e) {
        tests_failed_++;
        std::cerr << "FAIL: " << suite_name << " - Exception: " << e.what() << std::endl;
      }
      
      int suite_tests = tests_run_ - initial_tests;
      int suite_failures = tests_failed_ - initial_failures;
      
      std::cout << "=== " << suite_name << " Summary: " 
                << (suite_tests - suite_failures) << "/" << suite_tests 
                << " tests passed ===" << std::endl;
    }
    
    static int print_summary() {
      std::cout << "\n=== Final Test Summary ===" << std::endl;
      std::cout << "Total tests run: " << tests_run_ << std::endl;
      std::cout << "Tests passed: " << (tests_run_ - tests_failed_) << std::endl;
      std::cout << "Tests failed: " << tests_failed_ << std::endl;
      
      if (tests_failed_ > 0) {
        std::cout << "\nFailed tests:" << std::endl;
        for (const auto& failure : failed_tests_) {
          std::cout << "  " << failure << std::endl;
        }
      }
      
      return tests_failed_ > 0 ? 1 : 0;
    }
    
    static void reset() {
      tests_run_ = 0;
      tests_failed_ = 0;
      failed_tests_.clear();
    }
};

// Static member declarations (definitions will be in simple_test.cpp)

#define TEST_ASSERT_TRUE(condition, name) SimpleTest::assert_true(condition, name)
#define TEST_ASSERT_FALSE(condition, name) SimpleTest::assert_false(condition, name)
#define TEST_ASSERT_EQ(expected, actual, name) SimpleTest::assert_equals(expected, actual, name)
#define RUN_TEST_SUITE(name, func) SimpleTest::run_test_suite(name, func)