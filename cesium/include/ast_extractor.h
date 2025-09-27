/**
 * @file ast_extractor.h
 * @brief AST-based extraction of code constructs from parsed source code
 */
#pragma once

#include <string>
#include <vector>
#include <optional>
#include "tree_sitter/api.h"

/**
 * @enum ConstructType
 * @brief Types of code constructs that can be extracted from source code
 */
enum class ConstructType {
  Function,      ///< Regular function
  Method,        ///< Class/struct method
  Class,         ///< Class definition
  Struct,        ///< Struct definition
  Enum,          ///< Enumeration
  Variable,      ///< Variable declaration
  Namespace,     ///< Namespace definition
  Constructor,   ///< Class constructor
  Destructor     ///< Class destructor
};

/**
 * @struct Parameter
 * @brief Represents a function or method parameter
 */
struct Parameter {
  std::string type;                           ///< Parameter type
  std::string name;                           ///< Parameter name
  std::optional<std::string> default_value;  ///< Default value if present
};

/**
 * @struct CodeConstruct
 * @brief Complete representation of a code construct extracted from AST
 */
struct CodeConstruct {
  ConstructType type;                            ///< Type of construct (function, class, etc.)
  std::string name;                              ///< Simple name of the construct
  std::string full_name;                         ///< Full qualified name including namespace/class path
  std::string namespace_path;                    ///< Namespace or class containing this construct
  
  // Function/method specific fields
  std::optional<std::string> return_type;       ///< Return type for functions/methods
  std::vector<Parameter> parameters;            ///< Function/method parameters
  bool is_static = false;                       ///< Whether function/method is static
  bool is_const = false;                        ///< Whether method is const
  bool is_virtual = false;                      ///< Whether method is virtual
  
  // Class/struct specific fields
  std::vector<std::string> base_classes;        ///< Base classes for inheritance
  
  // Universal fields for all constructs
  std::string access_modifier;                  ///< Access level: public, private, protected
  std::optional<std::string> docstring;         ///< Associated docstring if found nearby
  uint32_t start_line;                          ///< Starting line number in source
  uint32_t end_line;                            ///< Ending line number in source
  std::string filename;                         ///< Source filename
  
  TSNode ast_node;                              ///< Raw Tree-sitter AST node for further processing
};

/**
 * @class ASTExtractor
 * @brief Extracts code constructs from Tree-sitter AST nodes
 * 
 * Traverses AST and identifies functions, classes, structs, enums, variables,
 * and namespaces, extracting their metadata and structure information.
 */
class ASTExtractor {
  public:
    /**
     * @brief Extract all code constructs from a parsed Tree-sitter tree
     * @param tree Parsed Tree-sitter tree
     * @param content Original source code content
     * @param filename Name of the source file
     * @return Vector of all extracted code constructs
     */
    std::vector<CodeConstruct> extractConstructs(TSTree* tree, const std::string& content, const std::string& filename);
    
  private:
    /**
     * @brief Recursively extract constructs from an AST node
     * @param node Current AST node to process
     * @param content Original source code content
     * @param filename Name of the source file
     * @param namespace_path Current namespace path
     * @param constructs Output vector to append constructs to
     */
    void extractFromNode(TSNode node, const std::string& content, const std::string& filename, 
                        const std::string& namespace_path, std::vector<CodeConstruct>& constructs);
    
    /**
     * @brief Extract function construct from AST node
     */
    CodeConstruct extractFunction(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path);
    
    /**
     * @brief Extract class construct from AST node
     */
    CodeConstruct extractClass(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path);
    
    /**
     * @brief Extract struct construct from AST node
     */
    CodeConstruct extractStruct(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path);
    
    /**
     * @brief Extract enum construct from AST node
     */
    CodeConstruct extractEnum(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path);
    
    /**
     * @brief Extract variable construct from AST node
     */
    CodeConstruct extractVariable(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path);
    
    /**
     * @brief Extract namespace construct from AST node
     */
    CodeConstruct extractNamespace(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path);
    
    /**
     * @brief Extract return type from function AST node
     */
    std::string extractReturnType(TSNode function_node, const std::string& content);
    
    /**
     * @brief Extract parameter list from function AST node
     */
    std::vector<Parameter> extractParameters(TSNode function_node, const std::string& content);
    
    /**
     * @brief Extract type name from type AST node
     */
    std::string extractTypeName(TSNode type_node, const std::string& content);
    
    /**
     * @brief Get text content of an AST node
     */
    std::string getNodeText(TSNode node, const std::string& content);
    
    /**
     * @brief Find docstring near an AST node (above or below)
     * @param node AST node to search near
     * @param content Original source code content
     * @return Optional docstring if found within proximity threshold
     */
    std::optional<std::string> findNearbyDocstring(TSNode node, const std::string& content);
    
    /**
     * @brief Find first child node with specified type
     */
    TSNode findChildByType(TSNode parent, const std::string& type);
    
    /**
     * @brief Find all child nodes with specified type
     */
    std::vector<TSNode> findChildrenByType(TSNode parent, const std::string& type);
};