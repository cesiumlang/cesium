#include "simple_test.h"

// Static member definitions
int SimpleTest::tests_run_ = 0;
int SimpleTest::tests_failed_ = 0;
std::vector<std::string> SimpleTest::failed_tests_;