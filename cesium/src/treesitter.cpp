/**
 * @file treesitter.cpp
 * @brief Dynamic Tree-sitter language parser loading implementation
 */
#include "treesitter.h"
#include <algorithm>
#include <iostream>
#include <filesystem>

bool DynamicLanguageLoader::loadLanguage(const std::string& name, const JsonValue& config) {
  std::string lib_path = config["library"].asString();
  std::string func_name = config["function"].asString();

  // Try to load the library
  dynlib::Library lib = dynlib::loadLibraryFromPaths(lib_path);
  if (!lib.isValid()) {
    std::cerr << "Failed to load " << lib_path << ": " << dynlib::getLastError() << std::endl;
    return false;
  }

  // Get the language function
  auto lang_func = lib.getFunction<TSLanguage*(*)()>(func_name);
  if (!lang_func) {
    std::cerr << "Failed to find function " << func_name << " in " << lib.getPath() << std::endl;
    return false;
  }

  // Create language info and move the library into it
  LanguageInfo info{
    .library = std::move(lib),
    .language = lang_func(),
    .extensions = config["extensions"].asStringArray(),
    .javadoc_style = config["javadoc_style"].asString(),
    .function_name = func_name
  };

  loaded_languages_[name] = std::move(info);
  return true;
}

std::pair<std::string, const LanguageInfo*> DynamicLanguageLoader::getLanguageForFile(const std::string& filename) {
  std::string ext = std::filesystem::path(filename).extension().string();

  for (const auto& [name, info] : loaded_languages_) {
    if (std::find(info.extensions.begin(), info.extensions.end(), ext) != info.extensions.end()) {
      return {name, &info};
    }
  }
  return {"", nullptr};
}

const std::map<std::string, LanguageInfo>& DynamicLanguageLoader::getLoadedLanguages() const {
  return loaded_languages_;
}
