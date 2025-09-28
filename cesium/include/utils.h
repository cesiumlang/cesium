#pragma once

#include <limits>
#include <chrono>
#include <string>
#include <vector>
#include <deque>

#define CAT(a, b) a##b

typedef std::vector<std::string> StrVec;
typedef std::deque<std::string> StrDeq;
typedef std::chrono::steady_clock::time_point Epoch;

template <typename T> const T inf = std::numeric_limits<T>::max();

StrVec tokenize_string(const std::string &s, const char delim = ' ');
StrDeq tokenize_string_deque(const std::string &s, const char delim = ' ');
const std::string trim_final_char(const std::string &s);
bool is_numeric(const char c);
Epoch tic();
double toc(Epoch t0);
