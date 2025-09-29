/**
@brief Dynamic Tree-sitter language parser loading implementation
*/
#include <backend/doc/treesitter.h>
#include <algorithm>
// #include <iostream>
#include <filesystem>
#include <backend/core/cli_utils.h>

bool DynamicLanguageLoader::loadLanguage(const std::string& name, const JsonValue& config, const std::string& configFilePath) {
  CLILogger::debug("DynamicLanguageLoader::loadLanguage: Loading language '" + name + "' from config");
  
  std::string lib_path = config["library"].asString();
  std::string func_name = config["function"].asString();
  
  CLILogger::debug("DynamicLanguageLoader::loadLanguage: Library path: '" + lib_path + "', function: '" + func_name + "'");

  // Validate config has required fields
  if (lib_path.empty()) {
    CLILogger::error("DynamicLanguageLoader::loadLanguage: Empty library path for language: " + name);
    return false;
  }
  
  if (func_name.empty()) {
    CLILogger::error("DynamicLanguageLoader::loadLanguage: Empty function name for language: " + name);
    return false;
  }

  // Try to load the library using config-based search strategy
  CLILogger::debug("DynamicLanguageLoader::loadLanguage: Attempting to load library for language: " + name);
  dynlib::DynLib lib = dynlib::loadDynLibFromConfig(lib_path, configFilePath);
  if (!lib.isValid()) {
    CLILogger::error("Failed to load " + lib_path + ": " + dynlib::getLastDynLibError());
    CLILogger::debug("DynamicLanguageLoader::loadLanguage: Library loading failed for language: " + name);
    return false;
  }
  
  CLILogger::debug("DynamicLanguageLoader::loadLanguage: Successfully loaded library: " + lib.getPath());

  // Get the language function
  CLILogger::debug("DynamicLanguageLoader::loadLanguage: Looking up tree-sitter function: " + func_name);
  auto lang_func = lib.getFunc<TSLanguage*(*)()>(func_name);
  if (!lang_func) {
    CLILogger::error("Failed to find function " + func_name + " in " + lib.getPath());
    CLILogger::debug("DynamicLanguageLoader::loadLanguage: Function lookup failed for language: " + name);
    return false;
  }
  
  CLILogger::debug("DynamicLanguageLoader::loadLanguage: Successfully found function: " + func_name);

  // Call the language function to get the TSLanguage*
  CLILogger::debug("DynamicLanguageLoader::loadLanguage: Calling tree-sitter language function: " + func_name);
  TSLanguage* ts_language = lang_func();
  if (!ts_language) {
    CLILogger::error("DynamicLanguageLoader::loadLanguage: Language function '" + func_name + "' returned null");
    return false;
  }
  
  CLILogger::debug("DynamicLanguageLoader::loadLanguage: Successfully created tree-sitter language object");

  // Extract and validate extensions array
  std::vector<std::string> extensions = config["extensions"].asStringArray();
  if (extensions.empty()) {
    CLILogger::warning("DynamicLanguageLoader::loadLanguage: No extensions specified for language: " + name);
  } else {
    CLILogger::debug("DynamicLanguageLoader::loadLanguage: Extensions for " + name + ": " + std::to_string(extensions.size()) + " extensions");
  }
  
  std::string docstring_style = config["docstring_style"].asString();
  CLILogger::debug("DynamicLanguageLoader::loadLanguage: Docstring style: '" + docstring_style + "'");

  // Create language info and move the library into it
  LanguageInfo info{
    .library = std::move(lib),
    .language = ts_language,
    .extensions = extensions,
    .docstring_style = docstring_style,
    .function_name = func_name
  };

  loaded_languages_[name] = std::move(info);
  CLILogger::debug("DynamicLanguageLoader::loadLanguage: Successfully loaded and registered language: " + name);
  return true;
}

std::pair<std::string, const LanguageInfo*> DynamicLanguageLoader::getLanguageForFile(const std::string& filename) {
  std::string ext = std::filesystem::path(filename).extension().string();
  CLILogger::debug("DynamicLanguageLoader::getLanguageForFile: Looking for language for file: " + filename + " (extension: '" + ext + "')");

  if (ext.empty()) {
    CLILogger::debug("DynamicLanguageLoader::getLanguageForFile: File has no extension, cannot match language");
    return {"", nullptr};
  }

  for (const auto& [name, info] : loaded_languages_) {
    CLILogger::debug("DynamicLanguageLoader::getLanguageForFile: Checking language '" + name + "' with " + std::to_string(info.extensions.size()) + " extensions");
    
    if (std::find(info.extensions.begin(), info.extensions.end(), ext) != info.extensions.end()) {
      CLILogger::debug("DynamicLanguageLoader::getLanguageForFile: Found matching language '" + name + "' for extension '" + ext + "'");
      return {name, &info};
    }
  }
  
  CLILogger::debug("DynamicLanguageLoader::getLanguageForFile: No language found for extension '" + ext + "' (checked " + std::to_string(loaded_languages_.size()) + " languages)");
  return {"", nullptr};
}

const std::map<std::string, LanguageInfo>& DynamicLanguageLoader::getLoadedLanguages() const {
  return loaded_languages_;
}
