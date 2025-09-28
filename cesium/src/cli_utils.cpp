/**
@brief Implementation of CLI utility functions
*/
#include "cli_utils.h"
#include <algorithm>
#include <iostream>
#include <string>

CommandArgParser::CommandArgParser(int argc, char* argv[], const std::string& expected_command) {
  (void)expected_command;

  // Find starting index after skipping program name and command structure
  int start_index = 1;
  if (argc > 1 && std::string(argv[0]) == "doc") {
    start_index = 2;
  } else if (argc > 2 && std::string(argv[1]) == "doc") {
    start_index = 3;
  }

  // Parse arguments
  for (int i = start_index; i < argc; i++) {
    std::string arg = argv[i];

    if (arg.starts_with("--") && i + 1 < argc && !std::string(argv[i + 1]).starts_with("-")) {
      // Option with value: --option value
      options_[arg] = argv[i + 1];
      i++; // Skip the value
    } else if (arg.starts_with("-")) {
      // Flag: -h, --help, etc.
      flags_.push_back(arg);
    } else {
      // Positional argument
      positional_.push_back(arg);
    }
  }
}

bool CommandArgParser::hasFlag(const std::string& flag) const {
  return std::find(flags_.begin(), flags_.end(), flag) != flags_.end();
}

std::string CommandArgParser::getOption(const std::string& option) const {
  auto it = options_.find(option);
  return it != options_.end() ? it->second : "";
}

std::vector<std::string> CommandArgParser::getPositionalArgs() const {
  return positional_;
}

