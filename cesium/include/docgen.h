/**
 * @file docgen.h
 * @brief Core documentation generation system for extracting and processing code documentation
 */
#pragma once

#include <string>
#include <vector>
#include "treesitter.h"
#include "javadoc.h"
#include "ts_ast_parser.h"
#include "ast_extractor.h"
#include "markdowngen.h"

/**
 * @class CesiumDocExtractor
 * @brief Main orchestrator for documentation generation from source code
 * 
 * Coordinates between Tree-sitter parsing, AST extraction, docstring parsing,
 * and markdown generation to produce comprehensive documentation.
 */
class CesiumDocExtractor {
  private:
    DynamicLanguageLoader loader_;         ///< Handles dynamic loading of Tree-sitter language parsers
    JavadocParser javadoc_parser_;         ///< Parses Javadoc-style documentation blocks
    DocAssociator doc_associator_;         ///< Associates docstrings with code constructs
    ASTExtractor ast_extractor_;           ///< Extracts code constructs from AST
    MarkdownGenerator markdown_generator_; ///< Generates markdown documentation

    /**
     * @brief Extracts Javadoc blocks from a source file
     * @param filepath Path to the source file to parse
     * @param lang_info Language information for Tree-sitter parsing
     * @return Vector of extracted Javadoc blocks
     */
    std::vector<JavadocBlock> extractFromFile(const std::string& filepath,
                                             const LanguageInfo& lang_info);
    
    /**
     * @brief Extracts all code constructs from a source file using AST analysis
     * @param filepath Path to the source file to parse
     * @param lang_info Language information for Tree-sitter parsing
     * @return Vector of all discovered code constructs
     */
    std::vector<CodeConstruct> extractAllConstructs(const std::string& filepath,
                                                   const LanguageInfo& lang_info);

  public:
    /**
     * @brief Initializes the documentation extractor with configuration
     * @param config_path Path to the configuration file
     * @return True if initialization succeeded, false otherwise
     */
    bool initialize(const std::string& config_path);
    
    /**
     * @brief Extracts and generates documentation based on configuration
     * @param config_path Path to the configuration file specifying input/output
     */
    void extractDocs(const std::string& config_path);
};
