/**
@brief Core utility functions and type definitions for Cesium
*/
#pragma once

#include <limits>
#include <chrono>
#include <string>
#include <vector>
#include <deque>

/**
@brief Macro for token concatenation
@param a First token
@param b Second token
*/
#define CAT(a, b) a##b

/**
@brief String vector type alias for convenience
*/
typedef std::vector<std::string> StrVec;

/**
@brief String deque type alias for convenience
*/
typedef std::deque<std::string> StrDeq;

/**
@brief Time point type alias for timing measurements
*/
typedef std::chrono::steady_clock::time_point Epoch;

/**
@brief Template variable for numeric maximum value (infinity)
@tparam T Numeric type
*/
template <typename T> const T inf = std::numeric_limits<T>::max();

/**
@brief Tokenize a string into a vector using a delimiter
@param s String to tokenize
@param delim Delimiter character (default: space)
@return Vector of string tokens
*/
StrVec tokenize_string(const std::string &s, const char delim = ' ');

/**
@brief Tokenize a string into a deque using a delimiter
@param s String to tokenize
@param delim Delimiter character (default: space)
@return Deque of string tokens
*/
StrDeq tokenize_string_deque(const std::string &s, const char delim = ' ');

/**
@brief Remove the final character from a string
@param s Input string
@return String with last character removed
*/
const std::string trim_final_char(const std::string &s);

/**
@brief Check if a character is numeric (0-9)
@param c Character to check
@return True if character is a digit, false otherwise
*/
bool is_numeric(const char c);

/**
@brief Start a timing measurement
@return Time point representing the start time
*/
Epoch tic();

/**
@brief End a timing measurement and calculate elapsed time
@param t0 Start time point from tic()
@return Elapsed time in seconds as a double
*/
double toc(Epoch t0);
