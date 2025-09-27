/**
 * @file ts_ast_parser.h
 * @brief Tree-sitter AST parsing utilities for documentation association
 */
#pragma once

#include <string>
#include <vector>
#include "tree_sitter/api.h"
#include "javadoc.h"

/**
 * @class DocAssociator
 * @brief Associates Javadoc documentation blocks with AST nodes
 * 
 * Analyzes parsed documentation comments and connects them to the appropriate
 * code constructs in the Tree-sitter AST by proximity and context analysis.
 */
class DocAssociator {
public:
  /**
   * @brief Associate documentation blocks with corresponding AST nodes
   * @param javadoc_blocks Vector of parsed documentation blocks to associate
   * @param tree Tree-sitter AST tree
   * @param content Original source code content
   */
  void associateDocsWithNodes(std::vector<JavadocBlock>& javadoc_blocks,
                              TSTree* tree, const std::string& content);

private:
  /**
   * @brief Find AST declaration node following a documentation comment
   */
  TSNode findFollowingDeclaration(TSTree* tree, TSNode root, SourceLocation javadoc_loc);
  
  /**
   * @brief Extract namespace/class path from AST node
   */
  std::string extractNamespacePath(TSNode node, const std::string& content);
  
  /**
   * @brief Extract symbol name from AST node
   */
  std::string extractSymbolName(TSNode node, const std::string& content);
  
  /**
   * @brief Get name node from declaration node
   */
  TSNode getNameNode(TSNode node);
  
  /**
   * @brief Extract text content from AST node
   */
  std::string getNodeText(TSNode node, const std::string& content);
  
  /**
   * @brief Join path components with separator
   */
  std::string joinPath(const std::vector<std::string>& parts, const std::string& separator);
};
