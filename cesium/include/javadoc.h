/**
 * @file javadoc.h
 * @brief Javadoc-style documentation block parsing and extraction
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include "treesitter.h"
#include "tree_sitter/api.h"

/**
 * @struct JavadocBlock
 * @brief Parsed Javadoc-style documentation block
 * 
 * Contains all extracted information from a documentation comment including
 * description, parameter documentation, return value info, and source location.
 */
struct JavadocBlock {
  std::string raw_content;                     ///< Original raw comment text
  std::string description;                     ///< Main description text
  std::map<std::string, std::string> params;   ///< Parameter name -> description mapping
  std::string return_desc;                     ///< Return value description
  std::vector<std::string> tags;               ///< Other tags found in the block
  SourceLocation location;                     ///< Source location of the comment
  TSNode associated_node;                      ///< AST node this comment documents
  std::string namespace_path;                  ///< Namespace/class path of associated symbol
  std::string symbol_name;                     ///< Name of symbol being documented
  std::string symbol_type;                     ///< Type of symbol (function, class, etc.)
};

/**
 * @class JavadocParser
 * @brief Parser for extracting Javadoc-style documentation from source code
 * 
 * Supports multiple comment styles including block comments (slash-star style)
 * and line comments (triple-slash style) for different programming languages.
 */
class JavadocParser {
  public:
    /**
     * @brief Extract all Javadoc-style documentation blocks from source code
     * @param content Source code content to parse
     * @param style Comment style (block comment marker for block comments, triple-slash for line comments)
     * @return Vector of parsed documentation blocks
     */
    std::vector<JavadocBlock> extractJavadocBlocks(const std::string& content, const std::string& style);

  private:
    /**
     * @brief Extract block-style comments
     */
    std::vector<JavadocBlock> extractBlockComments(const std::string& content);
    
    /**
     * @brief Extract simple block comments without Javadoc parsing
     */
    std::vector<JavadocBlock> extractBlockCommentsSimple(const std::string& content);
    
    /**
     * @brief Extract line comments with specified prefix (e.g., "///")
     */
    std::vector<JavadocBlock> extractLineComments(const std::string& content, const std::string& prefix);
    
    /**
     * @brief Parse Javadoc tags and structure from raw comment content
     */
    JavadocBlock parseJavadocContent(const std::string& raw);
    
    /**
     * @brief Clean up comment syntax from raw content
     */
    std::string cleanJavadocContent(const std::string& raw);
    
    /**
     * @brief Trim whitespace from string
     */
    std::string trim(const std::string& str);
    
    /**
     * @brief Convert byte offset to line/column location
     */
    SourceLocation getSourceLocation(const std::string& content, size_t byte_offset);
};
