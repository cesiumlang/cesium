/**
 * @file treesitter.h
 * @brief Dynamic Tree-sitter language parser loading and management
 */
#pragma once

#include <map>
#include <string>
#include <vector>
#include "tree_sitter/api.h"
#include "dynlib.h"
#include "json.h"

/**
 * @struct SourceLocation
 * @brief Location information within source code
 */
struct SourceLocation {
  size_t line;         ///< Line number (1-based)
  size_t column;       ///< Column number (0-based)
  size_t byte_offset;  ///< Byte offset from start of file
};

/**
 * @struct LanguageInfo
 * @brief Complete information about a loaded Tree-sitter language parser
 */
struct LanguageInfo {
  dynlib::Library library;                ///< Dynamic library containing the parser
  TSLanguage* language;                   ///< Tree-sitter language parser
  std::vector<std::string> extensions;    ///< File extensions this parser handles
  std::string javadoc_style;              ///< Documentation comment style (e.g., "/**", "///")
  std::string function_name;              ///< Tree-sitter function name for this language
};

/**
 * @class DynamicLanguageLoader
 * @brief Manages dynamic loading and caching of Tree-sitter language parsers
 * 
 * Loads Tree-sitter parser libraries from configuration, caches them for reuse,
 * and provides file extension to language mapping.
 */
class DynamicLanguageLoader {
  private:
    std::map<std::string, LanguageInfo> loaded_languages_;  ///< Cache of loaded parsers

  public:
    /**
     * @brief Load a Tree-sitter language parser from configuration
     * @param name Language name (e.g., "cpp", "python")
     * @param config JSON configuration for the language parser
     * @return True if loading succeeded, false on error
     */
    bool loadLanguage(const std::string& name, const JsonValue& config);
    
    /**
     * @brief Find appropriate language parser for a source file
     * @param filename Path to source file
     * @return Pair of language name and LanguageInfo pointer, or empty if not found
     */
    std::pair<std::string, const LanguageInfo*> getLanguageForFile(const std::string& filename);
    
    /**
     * @brief Get all currently loaded language parsers
     * @return Map of language names to their information
     */
    const std::map<std::string, LanguageInfo>& getLoadedLanguages() const;
};
