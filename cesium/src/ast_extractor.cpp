/**
 * @file ast_extractor.cpp
 * @brief AST-based code construct extraction implementation
 */
#include "ast_extractor.h"
#include <cstring>
#include <algorithm>

std::vector<CodeConstruct> ASTExtractor::extractConstructs(TSTree* tree, const std::string& content, const std::string& filename) {
  std::vector<CodeConstruct> constructs;
  TSNode root = ts_tree_root_node(tree);
  
  extractFromNode(root, content, filename, "", constructs);
  return constructs;
}

void ASTExtractor::extractFromNode(TSNode node, const std::string& content, const std::string& filename,
                                  const std::string& namespace_path, std::vector<CodeConstruct>& constructs) {
  std::string node_type = ts_node_type(node);
  
  // Extract this node if it's a construct we care about
  if (node_type == "function_definition") {
    constructs.push_back(extractFunction(node, content, filename, namespace_path));
  } else if (node_type == "class_specifier") {
    constructs.push_back(extractClass(node, content, filename, namespace_path));
  } else if (node_type == "struct_specifier") {
    constructs.push_back(extractStruct(node, content, filename, namespace_path));
  } else if (node_type == "enum_specifier") {
    constructs.push_back(extractEnum(node, content, filename, namespace_path));
  } else if (node_type == "namespace_definition") {
    constructs.push_back(extractNamespace(node, content, filename, namespace_path));
  }
  
  // Recursively process child nodes
  uint32_t child_count = ts_node_child_count(node);
  for (uint32_t i = 0; i < child_count; i++) {
    TSNode child = ts_node_child(node, i);
    
    // Update namespace path for namespace children
    std::string child_namespace_path = namespace_path;
    if (node_type == "namespace_definition") {
      TSNode name_node = findChildByType(node, "identifier");
      if (!ts_node_is_null(name_node)) {
        std::string ns_name = getNodeText(name_node, content);
        child_namespace_path = namespace_path.empty() ? ns_name : namespace_path + "::" + ns_name;
      }
    }
    
    extractFromNode(child, content, filename, child_namespace_path, constructs);
  }
}

CodeConstruct ASTExtractor::extractFunction(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path) {
  CodeConstruct construct;
  construct.type = ConstructType::Function;
  construct.ast_node = node;
  construct.filename = filename;
  construct.namespace_path = namespace_path;
  
  // Get function name
  TSNode declarator = findChildByType(node, "function_declarator");
  if (!ts_node_is_null(declarator)) {
    TSNode name_node = findChildByType(declarator, "identifier");
    if (!ts_node_is_null(name_node)) {
      construct.name = getNodeText(name_node, content);
      construct.full_name = namespace_path.empty() ? construct.name : namespace_path + "::" + construct.name;
    }
  }
  
  // Extract return type
  construct.return_type = extractReturnType(node, content);
  
  // Extract parameters
  construct.parameters = extractParameters(node, content);
  
  // Get line numbers
  TSPoint start_point = ts_node_start_point(node);
  TSPoint end_point = ts_node_end_point(node);
  construct.start_line = start_point.row + 1;  // Convert to 1-based
  construct.end_line = end_point.row + 1;
  
  // Look for nearby docstring
  construct.docstring = findNearbyDocstring(node, content);
  
  return construct;
}

CodeConstruct ASTExtractor::extractClass(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path) {
  CodeConstruct construct;
  construct.type = ConstructType::Class;
  construct.ast_node = node;
  construct.filename = filename;
  construct.namespace_path = namespace_path;
  
  // Get class name
  TSNode name_node = findChildByType(node, "type_identifier");
  if (!ts_node_is_null(name_node)) {
    construct.name = getNodeText(name_node, content);
    construct.full_name = namespace_path.empty() ? construct.name : namespace_path + "::" + construct.name;
  }
  
  // Get line numbers
  TSPoint start_point = ts_node_start_point(node);
  TSPoint end_point = ts_node_end_point(node);
  construct.start_line = start_point.row + 1;
  construct.end_line = end_point.row + 1;
  
  // Look for nearby docstring
  construct.docstring = findNearbyDocstring(node, content);
  
  return construct;
}

CodeConstruct ASTExtractor::extractStruct(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path) {
  CodeConstruct construct;
  construct.type = ConstructType::Struct;
  construct.ast_node = node;
  construct.filename = filename;
  construct.namespace_path = namespace_path;
  
  // Get struct name
  TSNode name_node = findChildByType(node, "type_identifier");
  if (!ts_node_is_null(name_node)) {
    construct.name = getNodeText(name_node, content);
    construct.full_name = namespace_path.empty() ? construct.name : namespace_path + "::" + construct.name;
  }
  
  // Get line numbers
  TSPoint start_point = ts_node_start_point(node);
  TSPoint end_point = ts_node_end_point(node);
  construct.start_line = start_point.row + 1;
  construct.end_line = end_point.row + 1;
  
  // Look for nearby docstring
  construct.docstring = findNearbyDocstring(node, content);
  
  return construct;
}

CodeConstruct ASTExtractor::extractEnum(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path) {
  CodeConstruct construct;
  construct.type = ConstructType::Enum;
  construct.ast_node = node;
  construct.filename = filename;
  construct.namespace_path = namespace_path;
  
  // Get enum name
  TSNode name_node = findChildByType(node, "type_identifier");
  if (!ts_node_is_null(name_node)) {
    construct.name = getNodeText(name_node, content);
    construct.full_name = namespace_path.empty() ? construct.name : namespace_path + "::" + construct.name;
  }
  
  // Get line numbers
  TSPoint start_point = ts_node_start_point(node);
  TSPoint end_point = ts_node_end_point(node);
  construct.start_line = start_point.row + 1;
  construct.end_line = end_point.row + 1;
  
  // Look for nearby docstring
  construct.docstring = findNearbyDocstring(node, content);
  
  return construct;
}

CodeConstruct ASTExtractor::extractNamespace(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path) {
  CodeConstruct construct;
  construct.type = ConstructType::Namespace;
  construct.ast_node = node;
  construct.filename = filename;
  construct.namespace_path = namespace_path;
  
  // Get namespace name
  TSNode name_node = findChildByType(node, "identifier");
  if (!ts_node_is_null(name_node)) {
    construct.name = getNodeText(name_node, content);
    construct.full_name = namespace_path.empty() ? construct.name : namespace_path + "::" + construct.name;
  }
  
  // Get line numbers
  TSPoint start_point = ts_node_start_point(node);
  TSPoint end_point = ts_node_end_point(node);
  construct.start_line = start_point.row + 1;
  construct.end_line = end_point.row + 1;
  
  // Look for nearby docstring
  construct.docstring = findNearbyDocstring(node, content);
  
  return construct;
}

std::string ASTExtractor::extractReturnType(TSNode function_node, const std::string& content) {
  // Look for primitive_type, type_identifier, or other type nodes before the function_declarator
  uint32_t child_count = ts_node_child_count(function_node);
  for (uint32_t i = 0; i < child_count; i++) {
    TSNode child = ts_node_child(function_node, i);
    std::string child_type = ts_node_type(child);
    
    if (child_type == "primitive_type" || child_type == "type_identifier" || 
        child_type == "qualified_identifier" || child_type == "template_type") {
      return getNodeText(child, content);
    }
    
    // Stop when we hit the function_declarator
    if (child_type == "function_declarator") {
      break;
    }
  }
  
  return "void";  // Default if no return type found
}

std::vector<Parameter> ASTExtractor::extractParameters(TSNode function_node, const std::string& content) {
  std::vector<Parameter> parameters;
  
  TSNode declarator = findChildByType(function_node, "function_declarator");
  if (ts_node_is_null(declarator)) return parameters;
  
  TSNode param_list = findChildByType(declarator, "parameter_list");
  if (ts_node_is_null(param_list)) return parameters;
  
  uint32_t child_count = ts_node_child_count(param_list);
  for (uint32_t i = 0; i < child_count; i++) {
    TSNode child = ts_node_child(param_list, i);
    if (std::string(ts_node_type(child)) == "parameter_declaration") {
      Parameter param;
      
      // Extract type (first non-comma child)
      TSNode type_node = ts_node_child(child, 0);
      if (!ts_node_is_null(type_node)) {
        param.type = extractTypeName(type_node, content);
      }
      
      // Extract name (look for identifier)
      TSNode name_node = findChildByType(child, "identifier");
      if (!ts_node_is_null(name_node)) {
        param.name = getNodeText(name_node, content);
      }
      
      // TODO: Extract default value if present
      
      parameters.push_back(param);
    }
  }
  
  return parameters;
}

std::string ASTExtractor::extractTypeName(TSNode type_node, const std::string& content) {
  std::string node_type = ts_node_type(type_node);
  
  if (node_type == "primitive_type" || node_type == "type_identifier") {
    return getNodeText(type_node, content);
  } else if (node_type == "pointer_declarator") {
    // Handle pointer types like "int*"
    TSNode base_type = ts_node_child(type_node, 0);
    if (!ts_node_is_null(base_type)) {
      return extractTypeName(base_type, content) + "*";
    }
  } else if (node_type == "reference_declarator") {
    // Handle reference types like "int&"
    TSNode base_type = ts_node_child(type_node, 0);
    if (!ts_node_is_null(base_type)) {
      return extractTypeName(base_type, content) + "&";
    }
  }
  
  // Fallback: return the raw text
  return getNodeText(type_node, content);
}

std::string ASTExtractor::getNodeText(TSNode node, const std::string& content) {
  uint32_t start = ts_node_start_byte(node);
  uint32_t end = ts_node_end_byte(node);
  if (end > content.length()) end = content.length();
  return content.substr(start, end - start);
}

std::optional<std::string> ASTExtractor::findNearbyDocstring(TSNode node, const std::string& content) {
  // Look for comment nodes before this node
  // This is a simplified implementation - could be enhanced
  
  uint32_t node_start = ts_node_start_byte(node);
  
  // Look backwards in content for /** or /// style comments
  if (node_start > 100) {  // Don't go too far back
    std::string before = content.substr(node_start - 100, 100);
    
    // Simple pattern matching for javadoc-style comments
    size_t javadoc_pos = before.rfind("/**");
    if (javadoc_pos != std::string::npos) {
      size_t end_pos = content.find("*/", node_start - 100 + javadoc_pos);
      if (end_pos != std::string::npos && end_pos < node_start) {
        return content.substr(node_start - 100 + javadoc_pos, end_pos - (node_start - 100 + javadoc_pos) + 2);
      }
    }
  }
  
  return std::nullopt;  // No docstring found
}

TSNode ASTExtractor::findChildByType(TSNode parent, const std::string& type) {
  uint32_t child_count = ts_node_child_count(parent);
  for (uint32_t i = 0; i < child_count; i++) {
    TSNode child = ts_node_child(parent, i);
    if (std::string(ts_node_type(child)) == type) {
      return child;
    }
  }
  return {};  // null node
}

std::vector<TSNode> ASTExtractor::findChildrenByType(TSNode parent, const std::string& type) {
  std::vector<TSNode> children;
  uint32_t child_count = ts_node_child_count(parent);
  for (uint32_t i = 0; i < child_count; i++) {
    TSNode child = ts_node_child(parent, i);
    if (std::string(ts_node_type(child)) == type) {
      children.push_back(child);
    }
  }
  return children;
}