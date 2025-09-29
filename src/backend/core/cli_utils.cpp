/**
@brief Implementation of CLI utility functions
*/
#include <backend/core/cli_utils.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <backend/core/logging.h>

CommandArgParser::CommandArgParser(int argc, char* argv[], const std::string& expected_command) {
  CLILogger::debug("CommandArgParser: Starting argument parsing with " + std::to_string(argc) + " arguments for command '" + expected_command + "'");
  (void)expected_command;

  // Find starting index after skipping program name and command structure
  int start_index = 1;
  if (argc > 1 && std::string(argv[0]) == "doc") {
    start_index = 2;
    CLILogger::debug("CommandArgParser: Detected 'doc' at argv[0], starting from index 2");
  } else if (argc > 2 && std::string(argv[1]) == "doc") {
    start_index = 3;
    CLILogger::debug("CommandArgParser: Detected 'doc' at argv[1], starting from index 3");
  } else {
    CLILogger::debug("CommandArgParser: No 'doc' command detected, starting from index 1");
  }

  // Parse arguments
  CLILogger::debug("CommandArgParser: Processing arguments from index " + std::to_string(start_index) + " to " + std::to_string(argc - 1));
  
  for (int i = start_index; i < argc; i++) {
    std::string arg = argv[i];
    CLILogger::debug("CommandArgParser: Processing argument[" + std::to_string(i) + "]: '" + arg + "'");

    if (arg.starts_with("--") && i + 1 < argc && !std::string(argv[i + 1]).starts_with("-")) {
      // Option with value: --option value
      std::string value = argv[i + 1];
      options_[arg] = value;
      CLILogger::debug("CommandArgParser: Added option '" + arg + "' with value '" + value + "'");
      i++; // Skip the value
    } else if (arg.starts_with("-")) {
      // Flag: -h, --help, etc.
      flags_.push_back(arg);
      CLILogger::debug("CommandArgParser: Added flag '" + arg + "'");
    } else {
      // Positional argument
      positional_.push_back(arg);
      CLILogger::debug("CommandArgParser: Added positional argument '" + arg + "'");
    }
  }
  
  CLILogger::debug("CommandArgParser: Parsing completed - " + std::to_string(options_.size()) + " options, " + std::to_string(flags_.size()) + " flags, " + std::to_string(positional_.size()) + " positional args");
}

bool CommandArgParser::hasFlag(const std::string& flag) const {
  bool found = std::find(flags_.begin(), flags_.end(), flag) != flags_.end();
  CLILogger::debug("CommandArgParser::hasFlag: Checking for flag '" + flag + "' - " + (found ? "found" : "not found"));
  return found;
}

std::string CommandArgParser::getOption(const std::string& option) const {
  auto it = options_.find(option);
  std::string value = it != options_.end() ? it->second : "";
  CLILogger::debug("CommandArgParser::getOption: Getting option '" + option + "' - value: '" + value + "'");
  return value;
}

std::vector<std::string> CommandArgParser::getPositionalArgs() const {
  CLILogger::debug("CommandArgParser::getPositionalArgs: Returning " + std::to_string(positional_.size()) + " positional arguments");
  return positional_;
}

