/**
 * @file markdowngen.h
 * @brief Markdown documentation file generation from parsed documentation and AST data
 */
#pragma once

#include <string>
#include <vector>
#include "javadoc.h"
#include "ast_extractor.h"

/**
 * @class MarkdownGenerator
 * @brief Generates markdown documentation files from Javadoc blocks and code constructs
 * 
 * Supports both traditional Javadoc-based documentation generation and modern
 * AST-centric documentation that creates docs for all code constructs.
 */
class MarkdownGenerator {
  public:
    /**
     * @brief Generate markdown files from traditional Javadoc blocks
     * @param blocks Vector of parsed Javadoc blocks
     * @param output_dir Directory to write markdown files to
     */
    void generateMarkdownFiles(const std::vector<JavadocBlock>& blocks,
                              const std::string& output_dir);
    
    /**
     * @brief Generate markdown files from AST-extracted code constructs
     * @param constructs Vector of code constructs from AST analysis
     * @param output_dir Directory to write markdown files to
     */
    void generateMarkdownFromConstructs(const std::vector<CodeConstruct>& constructs,
                                       const std::string& output_dir);

  private:
    // Traditional Javadoc-based generation methods
    
    /**
     * @brief Generate filename for a Javadoc block
     */
    std::string generateFilename(const JavadocBlock& block);
    
    /**
     * @brief Generate markdown file for a Javadoc block
     */
    void generateMarkdownFile(const JavadocBlock& block, const std::string& filepath);
    
    // Modern AST-based generation methods
    
    /**
     * @brief Generate filename for a code construct
     */
    std::string generateConstructFilename(const CodeConstruct& construct);
    
    /**
     * @brief Generate markdown file for a code construct
     */
    void generateConstructMarkdownFile(const CodeConstruct& construct, const std::string& filepath);
    
    /**
     * @brief Format construct type as human-readable string
     */
    std::string formatConstructType(ConstructType type);
    
    /**
     * @brief Format function signature with return type and parameters
     */
    std::string formatFunctionSignature(const CodeConstruct& construct);
    
    /**
     * @brief Format parameter list as markdown table
     */
    std::string formatParameters(const std::vector<Parameter>& parameters);
};
