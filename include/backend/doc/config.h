/**
@brief Configuration file loading and validation for Cesium documentation tools
*/
#pragma once

#include <string>
#include <memory>
#include <optional>
#include <backend/core/json.h>

namespace CesiumDoc {

/**
@brief Find the default configuration file in current directory

Searches for cesium-doc-config.jsonc first (preferred), then cesium-doc-config.json.
Issues a warning if both exist.

@return Path to default config file, or empty string if none found
*/
std::string findDefaultConfigFile();

/**
@brief Validate and resolve configuration file path

If config_path is explicitly provided, validates it exists and is a regular file.
If config_path is empty, attempts to find default configuration file.

@param config_path Input config path (may be empty for default lookup)
@param config_specified Whether config was explicitly specified by user
@return Resolved config path on success, empty string on error
*/
std::string validateAndResolveConfig(const std::string& config_path, bool config_specified);

/**
@brief Load and validate configuration file

Loads the JSON configuration file and performs basic validation.

@param config_path Path to configuration file
@return Loaded JsonDoc on success, std::nullopt on error
*/
std::optional<JsonDoc> loadConfig(const std::string& config_path);

} // namespace CesiumDoc