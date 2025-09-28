/**
@brief Utility classes and functions for command-line interface development
*/
#pragma once

#include <string>
#include <map>
#include <vector>

/**
@brief Simple command-line argument parser

Parses command-line arguments into flags, options (key-value pairs), and
positional arguments. Handles common CLI patterns like --option value and -h flags.
*/
class CommandArgParser {
  public:
    /**
    @brief Constructor that parses command-line arguments
    @param argc Argument count from main()
    @param argv Argument vector from main()
    @param expected_command Expected command name (used to skip command structure)
    */
    CommandArgParser(int argc, char* argv[], const std::string& expected_command);

    /**
    @brief Check if a flag is present
    @param flag Flag to check (e.g., "--help" or "-h")
    @return True if flag was provided
    */
    bool hasFlag(const std::string& flag) const;

    /**
    @brief Get value of an option
    @param option Option name (e.g., "--config")
    @return Option value or empty string if not provided
    */
    std::string getOption(const std::string& option) const;

    /**
    @brief Get positional arguments (non-flag, non-option arguments)
    @return Vector of positional argument strings
    */
    std::vector<std::string> getPositionalArgs() const;

  private:
    std::map<std::string, std::string> options_;   ///< Option name -> value mapping
    std::vector<std::string> flags_;               ///< List of flags provided
    std::vector<std::string> positional_;          ///< Positional arguments
};

#include "logging.h"

/**
@brief Backwards compatibility namespace that forwards to the new Logger namespace
@deprecated Use Logger namespace instead
*/
namespace CLILogger {
  // Forward calls to the new Logger namespace
  inline void configure(const LoggingConfig& config) { Logger::configure(config); }
  inline void configureFromJson(const std::string& json_str) { Logger::configureFromJson(json_str); }
  inline void configureFromFile(const std::string& config_file_path) { Logger::configureFromFile(config_file_path); }
  inline std::string getCurrentTimestamp() { return Logger::getCurrentTimestamp(); }
  inline MessageType resolveLogLevel(const std::string& level_name) { return Logger::resolveLogLevel(level_name); }
  inline bool shouldLog(MessageType type, bool for_console = true) { return Logger::shouldLog(type, for_console); }
  inline void log(MessageType type, const std::string& message) { Logger::log(type, message); }
  inline void log(const std::string& level_name, const std::string& message) { Logger::log(level_name, message); }
  inline void success(const std::string& message) { Logger::success(message); }
  inline void print(const std::string& message) { Logger::print(message); }
  inline void stderr_msg(const std::string& message) { Logger::stderr_msg(message); }
  inline void info(const std::string& message) { Logger::info(message); }
  inline void warning(const std::string& message) { Logger::warning(message); }
  inline void error(const std::string& message) { Logger::error(message); }
  inline void critical(const std::string& message) { Logger::critical(message); }
  inline void reserved(const std::string& message) { Logger::reserved(message); }
  inline void debug(const std::string& message) { Logger::debug(message); }
  inline void debuglow(const std::string& message) { Logger::debuglow(message); }
  inline void debuglow2(const std::string& message) { Logger::debuglow2(message); }
}
