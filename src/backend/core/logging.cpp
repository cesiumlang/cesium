/**
@brief Implementation of configurable logging system with file output and level filtering
*/
#include <backend/core/logging.h>
#include <backend/core/json.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cctype>

// Static configuration instance
static LoggingConfig g_logging_config;
static std::ofstream g_log_file;

std::string Logger::getCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

  std::stringstream ss;
  #ifdef _WIN32
    struct tm local_tm;
    localtime_s(&local_tm, &time_t);
    ss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
  #else
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  #endif
  ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
  return ss.str();
}

MessageType Logger::resolveLogLevel(const std::string& level_name) {
  // Convert to lowercase for case-insensitive comparison
  std::string lower_name = level_name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

  if (lower_name == "critical") return MessageType::Critical;
  if (lower_name == "error") return MessageType::Error;
  if (lower_name == "warning" || lower_name == "warn") return MessageType::Warning;
  if (lower_name == "stderr") return MessageType::Stderr;
  if (lower_name == "reserved" || lower_name == "tqdm") return MessageType::Reserved;
  if (lower_name == "success") return MessageType::Success;
  if (lower_name == "info") return MessageType::Info;
  if (lower_name == "print" || lower_name == "stdout") return MessageType::Print;
  if (lower_name == "debug") return MessageType::Debug;
  if (lower_name == "debuglow") return MessageType::DebugLow;
  if (lower_name == "debuglow2") return MessageType::DebugLow2;
  if (lower_name == "default" || lower_name == "notset" || lower_name == "not_set") return MessageType::Default;

  // If unrecognized, return Default
  return MessageType::Default;
}

void Logger::configure(const LoggingConfig& config) {
  g_logging_config = config;

  // Close existing log file if open
  if (g_log_file.is_open()) {
    g_log_file.close();
  }

  // Open new log file if specified
  if (!config.log_file.empty()) {
    // Create directory if it doesn't exist
    std::filesystem::path log_path(config.log_file);
    if (log_path.has_parent_path()) {
      std::filesystem::create_directories(log_path.parent_path());
    }

    g_log_file.open(config.log_file, std::ios::app);
    if (!g_log_file.is_open()) {
      std::cerr << "Warning: Failed to open log file: " << config.log_file << std::endl;
    }
  }
}

void Logger::configureFromFile(const std::string& config_file_path) {
  try {
    auto config_opt = JsonDoc::fromFile(config_file_path);
    if (!config_opt) {
      std::cerr << "Warning: Failed to load configuration file: " << config_file_path << std::endl;
      return;
    }

    const JsonDoc& config = *config_opt;
    JsonValue logging = config["logging"];
    if (logging.isNull()) {
      return; // No logging configuration
    }

    LoggingConfig new_config;

    // Parse log levels
    if (!logging["console_level"].isNull()) {
      new_config.console_level = resolveLogLevel(logging["console_level"].asString());
    }
    if (!logging["file_level"].isNull()) {
      new_config.file_level = resolveLogLevel(logging["file_level"].asString());
    }

    // Parse file settings
    if (!logging["log_file"].isNull()) {
      new_config.log_file = logging["log_file"].asString();
    }
    if (!logging["max_file_size_mb"].isNull()) {
      new_config.max_file_size_mb = logging["max_file_size_mb"].asInt();
    }
    if (!logging["backup_count"].isNull()) {
      new_config.backup_count = logging["backup_count"].asInt();
    }

    // Parse boolean settings
    if (!logging["enable_colors"].isNull()) {
      new_config.enable_colors = logging["enable_colors"].asBool();
    }
    if (!logging["enable_timestamps"].isNull()) {
      new_config.enable_timestamps = logging["enable_timestamps"].asBool();
    }

    configure(new_config);
  } catch (const std::exception& e) {
    std::cerr << "Warning: Failed to read configuration file: " << e.what() << std::endl;
  }
}

void Logger::configureFromJson(const std::string& json_str) {
  (void)json_str;
  // For now, this function is not needed since we don't have fromString
  // We can implement it later if needed using yyjson directly
  std::cerr << "Warning: configureFromJson not implemented - use configureFromFile instead" << std::endl;
}

bool Logger::shouldLog(MessageType type, bool for_console) {
  int message_level = static_cast<int>(type);
  int threshold_level = for_console ?
    static_cast<int>(g_logging_config.console_level) :
    static_cast<int>(g_logging_config.file_level);

  return message_level >= threshold_level;
}

void Logger::log(const std::string& level_name, const std::string& message) {
  MessageType type = resolveLogLevel(level_name);
  log(type, message);
}

void Logger::log(MessageType type, const std::string& message) {
  std::string timestamp = g_logging_config.enable_timestamps ? getCurrentTimestamp() : "";
  bool use_colors = g_logging_config.enable_colors;

  // Console output
  if (shouldLog(type, true)) {
    switch (type) {
      case MessageType::Critical:
        std::cerr << (use_colors ? ConsoleColors::magenta_text : "")
                  << (timestamp.empty() ? "" : timestamp + " ") << "CRITICAL: " << message
                  << (use_colors ? ConsoleColors::reset : "") << std::endl;
        break;

      case MessageType::Error:
        std::cerr << (use_colors ? ConsoleColors::red_text : "")
                  << (timestamp.empty() ? "" : timestamp + " ") << "ERROR: " << message
                  << (use_colors ? ConsoleColors::reset : "") << std::endl;
        break;

      case MessageType::Warning:
        std::cerr << (use_colors ? ConsoleColors::yellow_text : "")
                  << (timestamp.empty() ? "" : timestamp + " ") << "WARNING: " << message
                  << (use_colors ? ConsoleColors::reset : "") << std::endl;
        break;

      case MessageType::Stderr:
        std::cerr << (use_colors ? ConsoleColors::cyan_text : "") << message
                  << (use_colors ? ConsoleColors::reset : "") << std::endl;
        break;

      case MessageType::Reserved:
        std::cout << (use_colors ? ConsoleColors::blue_text : "") << message
                  << (use_colors ? ConsoleColors::reset : "") << std::endl;
        break;

      case MessageType::Success:
        std::cout << (use_colors ? ConsoleColors::green_text : "") << message
                  << (use_colors ? ConsoleColors::reset : "") << std::endl;
        break;

      case MessageType::Info:
        std::cout << (use_colors ? ConsoleColors::white_text : "")
                  << (timestamp.empty() ? "" : timestamp + " ") << "INFO: " << message
                  << (use_colors ? ConsoleColors::reset : "") << std::endl;
        break;

      case MessageType::Print:
        std::cout << (use_colors ? ConsoleColors::white_text : "") << message
                  << (use_colors ? ConsoleColors::reset : "") << std::endl;
        break;

      case MessageType::Debug:
        std::cout << (use_colors ? ConsoleColors::orange_text : "")
                  << (timestamp.empty() ? "" : timestamp + " ") << "DEBUG: " << message
                  << (use_colors ? ConsoleColors::reset : "") << std::endl;
        break;

      case MessageType::DebugLow:
        std::cout << (use_colors ? ConsoleColors::purple_text : "")
                  << (timestamp.empty() ? "" : timestamp + " ") << "DEBUG: " << message
                  << (use_colors ? ConsoleColors::reset : "") << std::endl;
        break;

      case MessageType::DebugLow2:
        std::cout << (use_colors ? ConsoleColors::gray_text : "")
                  << (timestamp.empty() ? "" : timestamp + " ") << "DEBUG: " << message
                  << (use_colors ? ConsoleColors::reset : "") << std::endl;
        break;

      case MessageType::Default:
      default:
        std::cout << (timestamp.empty() ? "" : timestamp + " ") << "LOG: " << message << std::endl;
        break;
    }
  }

  // File output (always without colors)
  if (g_log_file.is_open() && shouldLog(type, false)) {
    std::string level_str;
    switch (type) {
      case MessageType::Critical: level_str = "CRITICAL"; break;
      case MessageType::Error: level_str = "ERROR"; break;
      case MessageType::Warning: level_str = "WARNING"; break;
      case MessageType::Stderr: level_str = "STDERR"; break;
      case MessageType::Reserved: level_str = "RESERVED"; break;
      case MessageType::Success: level_str = "SUCCESS"; break;
      case MessageType::Info: level_str = "INFO"; break;
      case MessageType::Print: level_str = "PRINT"; break;
      case MessageType::Debug: level_str = "DEBUG"; break;
      case MessageType::DebugLow: level_str = "DEBUG_LOW"; break;
      case MessageType::DebugLow2: level_str = "DEBUG_LOW2"; break;
      case MessageType::Default: default: level_str = "LOG"; break;
    }

    g_log_file << (timestamp.empty() ? "" : timestamp + " ") << level_str << ": " << message << std::endl;
    g_log_file.flush(); // Ensure immediate write
  }
}

// Convenience functions
void Logger::success(const std::string& message) {
  log(MessageType::Success, message);
}

void Logger::print(const std::string& message) {
  log(MessageType::Print, message);
}

void Logger::stderr_msg(const std::string& message) {
  log(MessageType::Stderr, message);
}

void Logger::warning(const std::string& message) {
  log(MessageType::Warning, message);
}

void Logger::error(const std::string& message) {
  log(MessageType::Error, message);
}

void Logger::critical(const std::string& message) {
  log(MessageType::Critical, message);
}

void Logger::reserved(const std::string& message) {
  log(MessageType::Reserved, message);
}

void Logger::info(const std::string& message) {
  log(MessageType::Info, message);
}

void Logger::debug(const std::string& message) {
  log(MessageType::Debug, message);
}

void Logger::debuglow(const std::string& message) {
  log(MessageType::DebugLow, message);
}

void Logger::debuglow2(const std::string& message) {
  log(MessageType::DebugLow2, message);
}
