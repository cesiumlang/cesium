/**
@brief Markdown documentation file generation from parsed documentation and AST data
*/
#pragma once

#include <string>
#include <vector>
#include "cpp/docstrings.h"
#include "cpp/ast_extractor.h"

/**
@brief Generates markdown documentation files from docstring blocks and code constructs

Supports both traditional docstring-based documentation generation (Javadoc/Doxygen) and modern
AST-centric documentation that creates docs for all code constructs.
*/
class MarkdownGenerator {
  public:
    /**
    @brief Generate markdown files from traditional docstring blocks
    @param blocks Vector of parsed docstring blocks
    @param output_dir Directory to write markdown files to
    */
    void generateMarkdownFiles(const std::vector<DocstringBlock>& blocks,
                              const std::string& output_dir);
    
    /**
    @brief Generate markdown files from AST-extracted code constructs
    @param constructs Vector of code constructs from AST analysis
    @param output_dir Directory to write markdown files to
    @return Vector of generated file paths
    */
    std::vector<std::string> generateMarkdownFromConstructs(const std::vector<CodeConstruct>& constructs,
                                                           const std::string& output_dir);

  private:
    // Traditional docstring-based generation methods
    
    /**
    @brief Generate filename for a docstring block
    */
    std::string generateFilename(const DocstringBlock& block);
    
    /**
    @brief Generate markdown file for a docstring block
    */
    void generateMarkdownFile(const DocstringBlock& block, const std::string& filepath);
    
    // Modern AST-based generation methods
    
    /**
    @brief Generate filename for a code construct
    */
    std::string generateConstructFilename(const CodeConstruct& construct);
    
    /**
    @brief Generate markdown file for a code construct
    */
    void generateConstructMarkdownFile(const CodeConstruct& construct, const std::string& filepath);
    
    /**
    @brief Format construct type as human-readable string
    */
    std::string formatConstructType(ConstructType type);
    
    /**
    @brief Format function signature with return type and parameters
    */
    std::string formatFunctionSignature(const CodeConstruct& construct);
    
    /**
    @brief Format parameter list as markdown table
    */
    std::string formatParameters(const std::vector<Parameter>& parameters);
};
