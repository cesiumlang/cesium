/**
@brief Core documentation generation system for extracting and processing code documentation
*/
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <backend/doc/treesitter.h>
#include <backend/doc/cpp/docstrings.h>
#include <backend/doc/cpp/ts_ast_parser.h>
#include <backend/doc/cpp/ast_extractor.h>
#include <backend/doc/markdowngen.h>
#include <backend/doc/cache.h>

/**
@brief Main orchestrator for documentation generation from source code

Coordinates between Tree-sitter parsing, AST extraction, docstring parsing,
and markdown generation to produce comprehensive documentation.
*/
class CesiumDocExtractor {
  private:
    DynamicLanguageLoader loader_;         ///< Handles dynamic loading of Tree-sitter language parsers
    DocstringParser docstring_parser_;     ///< Parses documentation strings (Javadoc/Doxygen)
    DocAssociator doc_associator_;         ///< Associates docstrings with code constructs
    ASTExtractor ast_extractor_;           ///< Extracts code constructs from AST
    MarkdownGenerator markdown_generator_; ///< Generates markdown documentation
    std::unique_ptr<DocumentationCache> cache_; ///< Metadata cache for tracking file changes

    /**
    @brief Extracts docstring blocks from a source file
    @param filepath Path to the source file to parse
    @param lang_info Language information for Tree-sitter parsing
    @return Vector of extracted docstring blocks
    */
    std::vector<DocstringBlock> extractFromFile(const std::string& filepath,
                                             const LanguageInfo& lang_info);
    
    /**
    @brief Extracts all code constructs from a source file using AST analysis
    @param filepath Path to the source file to parse
    @param lang_info Language information for Tree-sitter parsing
    @return Vector of all discovered code constructs
    */
    std::vector<CodeConstruct> extractAllConstructs(const std::string& filepath,
                                                   const LanguageInfo& lang_info);
    
    /**
    @brief Checks if source file is newer than its corresponding markdown snippet
    @param source_path Path to source file
    @param extract_dir Directory containing markdown snippets
    @return True if source file needs re-extraction
    */
    bool needsExtraction(const std::string& source_path, const std::string& extract_dir);
    
    /**
    @brief Processes markdown snippets into structured documentation
    @param extract_dir Directory containing markdown snippets
    @param output_dir Directory for structured documentation output
    */
    void processMarkdownSnippets(const std::string& extract_dir, const std::string& output_dir);

  public:
    /**
    @brief Initializes the documentation extractor with configuration
    @param config_path Path to the configuration file
    @return True if initialization succeeded, false otherwise
    */
    bool initialize(const std::string& config_path);
    
    /**
    @brief Extracts docstrings and creates markdown snippets
    @param config_path Path to the configuration file
    @param source_override Optional source directory/file override
    @param extract_dir_override Optional extract directory override
    @return True if extraction succeeded, false otherwise
    */
    bool extract(const std::string& config_path, 
                 const std::string& source_override = "",
                 const std::string& extract_dir_override = "");
    
    /**
    @brief Generates structured documentation from extracted snippets
    @param config_path Path to the configuration file
    @return True if generation succeeded, false otherwise
    */
    bool generate(const std::string& config_path);
    
    /**
    @brief Legacy method - extracts and generates documentation (calls extract then generate)
    @param config_path Path to the configuration file specifying input/output
    */
    void extractDocs(const std::string& config_path);
};
