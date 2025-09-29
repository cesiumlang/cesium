/**
@brief Configuration file loading and validation implementation
*/
#include <backend/doc/config.h>
#include <backend/core/cli_utils.h>
#include <filesystem>
#include <iostream>

namespace CesiumDoc {

std::string findDefaultConfigFile() {
  CLILogger::debug("findDefaultConfigFile: Searching for default configuration files in current directory");
  
  // Check for both config files
  bool has_jsonc = std::filesystem::exists("cesium-doc-config.jsonc");
  bool has_json = std::filesystem::exists("cesium-doc-config.json");
  
  CLILogger::debug("findDefaultConfigFile: cesium-doc-config.jsonc exists: " + std::string(has_jsonc ? "true" : "false"));
  CLILogger::debug("findDefaultConfigFile: cesium-doc-config.json exists: " + std::string(has_json ? "true" : "false"));
  
  if (has_jsonc && has_json) {
    CLILogger::warning("Both cesium-doc-config.json and cesium-doc-config.jsonc exist. Using cesium-doc-config.jsonc (preferred)");
    CLILogger::debug("findDefaultConfigFile: Selected cesium-doc-config.jsonc due to preference");
    return "cesium-doc-config.jsonc";
  }
  
  if (has_jsonc) {
    CLILogger::debug("findDefaultConfigFile: Selected cesium-doc-config.jsonc");
    return "cesium-doc-config.jsonc";
  }
  
  if (has_json) {
    CLILogger::debug("findDefaultConfigFile: Selected cesium-doc-config.json");
    return "cesium-doc-config.json";
  }
  
  CLILogger::debug("findDefaultConfigFile: No default configuration files found");
  return "";
}

std::string validateAndResolveConfig(const std::string& config_path, bool config_specified) {
  CLILogger::debug("validateAndResolveConfig: Starting config resolution for path: '" + config_path + "', specified: " + std::string(config_specified ? "true" : "false"));
  
  std::string resolved_path = config_path;

  // If config was explicitly specified, validate it exists
  if (config_specified) {
    CLILogger::debug("validateAndResolveConfig: Validating explicitly specified config file: " + resolved_path);
    if (!std::filesystem::exists(resolved_path)) {
      CLILogger::error("validateAndResolveConfig: Specified configuration file does not exist: " + resolved_path);
      return "";
    }
    if (!std::filesystem::is_regular_file(resolved_path)) {
      CLILogger::error("validateAndResolveConfig: Specified configuration path is not a file: " + resolved_path);
      return "";
    }
    CLILogger::debug("validateAndResolveConfig: Successfully validated specified config file: " + resolved_path);
    return resolved_path;
  }

  // If no config was specified, check for default config file
  CLILogger::debug("validateAndResolveConfig: No config specified, searching for default config files");
  resolved_path = findDefaultConfigFile();
  if (!resolved_path.empty()) {
    std::string absolute_path = std::filesystem::absolute(resolved_path).string();
    CLILogger::print("Using default configuration file: " + absolute_path);
    CLILogger::debug("validateAndResolveConfig: Successfully resolved default config to: " + absolute_path);
    return resolved_path;
  } else {
    CLILogger::error("validateAndResolveConfig: No configuration file specified and no default config found.");
    CLILogger::stderr_msg("Use --config <file> or create cesium-doc-config.json or cesium-doc-config.jsonc in current directory.");
    return "";
  }
}

std::optional<JsonDoc> loadConfig(const std::string& config_path) {
  CLILogger::debug("loadConfig: Attempting to load configuration from: " + config_path);
  
  auto config = JsonDoc::fromFile(config_path);
  if (!config) {
    CLILogger::error("loadConfig: Failed to load configuration from: " + config_path);
    return config;
  }
  
  CLILogger::debug("loadConfig: Successfully loaded configuration from: " + config_path);
  return config;
}

} // namespace CesiumDoc