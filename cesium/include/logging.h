/**
@brief Configurable logging system with file output and level filtering
*/
#pragma once

#include <string>

/**
@brief ANSI color codes for terminal output formatting
*/
namespace ConsoleColors {
  const std::string reset = "\033[0m";
  const std::string red_text = "\033[31m";
  const std::string green_text = "\033[32m";
  const std::string yellow_text = "\033[33m";
  const std::string blue_text = "\033[34m";
  const std::string magenta_text = "\033[35m";
  const std::string cyan_text = "\033[36m";
  const std::string white_text = "\033[37m";
  const std::string gray_text = "\033[90m";     // For debug messages
}

/**
@brief Numerical log levels loosely matching PyRandyOS logging conventions
*/
namespace LogLevel {
  const int critical = 50;
  const int error = 40;
  const int warning = 30;
  const int stderr_level = warning - 1;    // 29
  const int stdout_level = 20;
  const int info = stdout_level + 1;       // 21
  const int success = stdout_level + 2;    // 22
  const int reserved = stdout_level + 3;   // 23 (formerly LOGTQDM)
  const int debug = 10;
  const int debug_low = debug - 1;         // 9
  const int debug_low2 = debug - 2;        // 8
  const int not_set = 0;                   // Default/undefined
}

/**
@brief Message types for CLI logging system with numerical values
*/
enum class MessageType {
  Critical = LogLevel::critical,     // 50 - Magenta - critical error messages with timestamp
  Error = LogLevel::error,           // 40 - Red - error messages with timestamp
  Warning = LogLevel::warning,       // 30 - Yellow - warning messages with timestamp
  Stderr = LogLevel::stderr_level,   // 29 - Cyan - default stderr messages
  Reserved = LogLevel::reserved,     // 23 - Blue - reserved for future use
  Success = LogLevel::success,       // 22 - Green - success messages
  Info = LogLevel::info,             // 21 - White - info messages with INFO: prefix
  Print = LogLevel::stdout_level,    // 20 - White - regular print statements
  Debug = LogLevel::debug,           // 10 - Gray - debug messages
  DebugLow = LogLevel::debug_low,    // 9 - Gray - low priority debug
  DebugLow2 = LogLevel::debug_low2,  // 8 - Gray - lowest priority debug
  Default = LogLevel::not_set        // 0 - No color - fallback for unrecognized types
};

/**
@brief Configurable logging system with file output and level filtering
*/
struct LoggingConfig {
  MessageType console_level = MessageType::Info;        ///< Minimum level for console output
  MessageType file_level = MessageType::Debug;          ///< Minimum level for file output
  std::string log_file = "";                           ///< Log file path (empty = no file logging)
  size_t max_file_size_mb = 10;                        ///< Maximum log file size in MB
  int backup_count = 5;                                ///< Number of backup files to keep
  bool enable_colors = true;                           ///< Enable ANSI color codes
  bool enable_timestamps = true;                       ///< Include timestamps in messages
};

/**
@brief Colored CLI logging system with timestamp formatting and file output
*/
namespace Logger {
  /**
  @brief Configure logging system with settings from config object
  @param config Logging configuration settings
  */
  void configure(const LoggingConfig& config);

  /**
  @brief Configure logging from JSON configuration object
  @param logging_json JSON object containing logging settings
  */
  void configureFromJson(const std::string& json_str);

  /**
  @brief Configure logging from JSON configuration file
  @param config_file_path Path to JSON configuration file
  */
  void configureFromFile(const std::string& config_file_path);

  /**
  @brief Get current timestamp string with millisecond precision
  @return Formatted timestamp string
  */
  std::string getCurrentTimestamp();

  /**
  @brief Resolve string log level name to MessageType (case-insensitive)
  @param level_name Log level name (e.g., "info", "WARNING", "Debug")
  @return Corresponding MessageType, or MessageType::Default if unrecognized
  */
  MessageType resolveLogLevel(const std::string& level_name);

  /**
  @brief Check if a message should be logged based on current level settings
  @param type Message type to check
  @param for_console True to check console level, false for file level
  @return True if message should be logged
  */
  bool shouldLog(MessageType type, bool for_console = true);

  /**
  @brief Print a message with appropriate formatting based on message type
  @param type Message type determining color and format
  @param message Message content to display
  */
  void log(MessageType type, const std::string& message);

  /**
  @brief Log a message using string level name (case-insensitive)
  @param level_name Log level name (e.g., "info", "warning", "error")
  @param message Message content to display
  */
  void log(const std::string& level_name, const std::string& message);

  /**
  @brief Convenience functions for common message types
  */
  void success(const std::string& message);
  void print(const std::string& message);
  void stderr_msg(const std::string& message);
  void info(const std::string& message);
  void warning(const std::string& message);
  void error(const std::string& message);
  void critical(const std::string& message);
  void reserved(const std::string& message);
  void debug(const std::string& message);
  void debuglow(const std::string& message);
  void debuglow2(const std::string& message);
}