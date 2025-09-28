/**
@brief Documentation string parsing and extraction

Supports multiple documentation formats including Javadoc, Doxygen, and similar styles.
*/
#pragma once

#include <string>
#include <vector>
#include <map>
#include "../treesitter.h"

/**
@brief Parsed documentation string block

Contains all extracted information from a documentation comment including
description, parameter documentation, return value info, and source location.
Supports multiple formats including Javadoc and Doxygen.
*/
struct DocstringBlock {
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
  
  // Optional override tags (can override AST-inferred information)
  std::string override_file;                   ///< Override for filename (from @file tag)
  std::string override_class;                  ///< Override for class name (from @class tag)
  std::string override_struct;                 ///< Override for struct name (from @struct tag)
  std::string override_enum;                   ///< Override for enum name (from @enum tag)
};

/**
@brief Parser for extracting documentation strings from source code

Supports multiple documentation formats (Javadoc, Doxygen) and comment styles
including block comments (slash-star style) and line comments (triple-slash style)
for different programming languages.
*/
class DocstringParser {
  public:
    /**
    @brief Extract all documentation string blocks from source code
    @param content Source code content to parse
    @param style Comment style (block comment marker for block comments, triple-slash for line comments)
    @return Vector of parsed documentation blocks
    */
    std::vector<DocstringBlock> extractDocstrings(const std::string& content, const std::string& style);

  private:
    /**
    @brief Extract block-style comments
    */
    std::vector<DocstringBlock> extractBlockComments(const std::string& content);
    
    /**
    @brief Extract simple block comments without full docstring parsing
    */
    std::vector<DocstringBlock> extractBlockCommentsSimple(const std::string& content);
    
    /**
    @brief Extract line comments with specified prefix (e.g., "///")
    */
    std::vector<DocstringBlock> extractLineComments(const std::string& content, const std::string& prefix);
    
    /**
    @brief Parse docstring tags and structure from raw comment content
    
    Supports both Javadoc (@param, @return) and Doxygen (\param, \return) syntax.
    */
    DocstringBlock parseDocstringContent(const std::string& raw);
    
    /**
    @brief Clean up comment syntax from raw content
    */
    std::string cleanDocstringContent(const std::string& raw);
    
    /**
    @brief Trim whitespace from string
    */
    std::string trim(const std::string& str);
    
    /**
    @brief Convert byte offset to line/column location
    */
    SourceLocation getSourceLocation(const std::string& content, size_t byte_offset);
};
