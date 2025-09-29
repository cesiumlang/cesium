/**
@brief AST-based code construct extraction implementation
*/
#include <backend/doc/cpp/ast_extractor.h>
// #include <cstring>
// #include <algorithm>
// #include <iostream>
#include <set>
#include <map>
#include <backend/core/cli_utils.h>

std::vector<CodeConstruct> ASTExtractor::extractConstructs(TSTree* tree, const std::string& content, const std::string& filename) {
  CLILogger::debug("ASTExtractor::extractConstructs: Starting extraction for " + filename);
  std::vector<CodeConstruct> constructs;
  TSNode root = ts_tree_root_node(tree);
  
  CLILogger::debug("ASTExtractor::extractConstructs: Root node type: " + std::string(ts_node_type(root)) + ", child count: " + std::to_string(ts_node_child_count(root)));

  extractFromNode(root, content, filename, "", constructs);
  
  CLILogger::debug("ASTExtractor::extractConstructs: Initial extraction found " + std::to_string(constructs.size()) + " constructs");

  // Merge duplicate constructs from declaration and implementation
  CLILogger::debug("ASTExtractor::extractConstructs: Starting duplicate construct merging");
  int conflicts = mergeDuplicateConstructs(constructs);
  if (conflicts > 0) {
    CLILogger::warning("Found " + std::to_string(conflicts) + " docstring conflicts during merging");
  }
  CLILogger::debug("ASTExtractor::extractConstructs: Merge completed, final count: " + std::to_string(constructs.size()) + " constructs");

  return constructs;
}

void ASTExtractor::extractFromNode(TSNode node, const std::string& content, const std::string& filename,
                                  const std::string& namespace_path, std::vector<CodeConstruct>& constructs) {
  std::string node_type = ts_node_type(node);
  
  // Log detailed information about nodes we're processing (but only for interesting node types to avoid spam)
  static const std::set<std::string> interesting_nodes = {
    "function_definition", "function_declarator", "declaration", "class_specifier", 
    "struct_specifier", "enum_specifier", "namespace_definition"
  };
  
  if (interesting_nodes.count(node_type)) {
    TSPoint start_point = ts_node_start_point(node);
    CLILogger::debug("ASTExtractor::extractFromNode: Processing " + node_type + " at line " + std::to_string(start_point.row + 1) + " in namespace '" + namespace_path + "'");
  }

  // Extract this node if it's a construct we care about
  if (node_type == "function_definition") {
    // Check if this is a deleted function (e.g., "= delete")
    std::string node_text = getNodeText(node, content);
    if (node_text.find("= delete") != std::string::npos) {
      CLILogger::debug("ASTExtractor::extractFromNode: Skipping deleted function at line " + std::to_string(ts_node_start_point(node).row + 1));
      return;
    }

    CLILogger::debug("Processing function_definition in " + filename + ", text preview: '" + node_text.substr(0, 50) + "...'");
    auto function_construct = extractFunction(node, content, filename, namespace_path);
    constructs.push_back(function_construct);
    CLILogger::debug("ASTExtractor::extractFromNode: Extracted function '" + function_construct.name + "' (" + function_construct.full_name + ")");
    // Don't recurse into function_definition children to avoid duplicate extraction
    return;
  } else if (node_type == "function_declarator") {
    // Handle method declarations directly
    auto method_construct = extractMethodDeclaration(node, content, filename, namespace_path);
    constructs.push_back(method_construct);
    CLILogger::debug("ASTExtractor::extractFromNode: Extracted method declaration '" + method_construct.name + "' (" + method_construct.full_name + ")");
    // Don't recurse into function_declarator children
    return;
  } else if (node_type == "declaration") {
    // Check if this declaration contains a function declarator (method declaration)
    TSNode declarator = findChildByType(node, "function_declarator");
    if (!ts_node_is_null(declarator)) {
      auto method_construct = extractMethodDeclaration(declarator, content, filename, namespace_path);
      constructs.push_back(method_construct);
      CLILogger::debug("ASTExtractor::extractFromNode: Extracted method declaration from general declaration '" + method_construct.name + "' (" + method_construct.full_name + ")");
      // Don't recurse into declaration children if we found a function declarator
      return;
    }
  } else if (node_type == "class_specifier") {
    auto class_construct = extractClass(node, content, filename, namespace_path);
    constructs.push_back(class_construct);
    CLILogger::debug("ASTExtractor::extractFromNode: Extracted class '" + class_construct.name + "' (" + class_construct.full_name + ")");
  } else if (node_type == "struct_specifier") {
    auto struct_construct = extractStruct(node, content, filename, namespace_path);
    constructs.push_back(struct_construct);
    CLILogger::debug("ASTExtractor::extractFromNode: Extracted struct '" + struct_construct.name + "' (" + struct_construct.full_name + ")");
  } else if (node_type == "enum_specifier") {
    auto enum_construct = extractEnum(node, content, filename, namespace_path);
    constructs.push_back(enum_construct);
    CLILogger::debug("ASTExtractor::extractFromNode: Extracted enum '" + enum_construct.name + "' (" + enum_construct.full_name + ")");
  } else if (node_type == "namespace_definition") {
    auto namespace_construct = extractNamespace(node, content, filename, namespace_path);
    constructs.push_back(namespace_construct);
    CLILogger::debug("ASTExtractor::extractFromNode: Extracted namespace '" + namespace_construct.name + "' (" + namespace_construct.full_name + ")");
  }

  // Recursively process child nodes
  uint32_t child_count = ts_node_child_count(node);
  if (interesting_nodes.count(node_type) && child_count > 0) {
    CLILogger::debug("ASTExtractor::extractFromNode: Recursively processing " + std::to_string(child_count) + " children of " + node_type);
  }
  for (uint32_t i = 0; i < child_count; i++) {
    TSNode child = ts_node_child(node, i);

    // Update namespace path for namespace and class children
    std::string child_namespace_path = namespace_path;
    if (node_type == "namespace_definition") {
      TSNode name_node = findChildByType(node, "identifier");
      if (!ts_node_is_null(name_node)) {
        std::string ns_name = getNodeText(name_node, content);
        child_namespace_path = namespace_path.empty() ? ns_name : namespace_path + "::" + ns_name;
      }
    } else if (node_type == "class_specifier" || node_type == "struct_specifier") {
      TSNode name_node = findChildByType(node, "type_identifier");
      if (!ts_node_is_null(name_node)) {
        std::string class_name = getNodeText(name_node, content);
        child_namespace_path = namespace_path.empty() ? class_name : namespace_path + "::" + class_name;
      }
    }

    extractFromNode(child, content, filename, child_namespace_path, constructs);
  }
  
  // Log completion for interesting nodes
  if (interesting_nodes.count(node_type)) {
    CLILogger::debug("ASTExtractor::extractFromNode: Completed processing " + node_type + ", total constructs so far: " + std::to_string(constructs.size()));
  }
}

CodeConstruct ASTExtractor::extractFunction(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path) {
  CodeConstruct construct;
  construct.type = ConstructType::Function;
  construct.ast_node = node;
  construct.filename = filename;
  construct.namespace_path = namespace_path;


  // Get function name and handle qualified identifiers (e.g., Class::method, Class::operator=)
  TSNode declarator = findChildByType(node, "function_declarator");
  if (!ts_node_is_null(declarator)) {
    CLILogger::debug("extractFunction: Found function_declarator");
    TSNode name_node = findChildByType(declarator, "qualified_identifier");
    if (!ts_node_is_null(name_node)) {
      // Handle qualified identifier like "JsonValue::asDouble" or "JsonDoc::operator="
      std::string full_qualified_name = getNodeText(name_node, content);
      CLILogger::debug("Found qualified_identifier: '" + full_qualified_name + "' in " + filename);


      size_t scope_pos = full_qualified_name.rfind("::");
      if (scope_pos != std::string::npos) {
        construct.namespace_path = full_qualified_name.substr(0, scope_pos);
        construct.name = full_qualified_name.substr(scope_pos + 2);
        construct.full_name = full_qualified_name;

      } else {
        construct.name = full_qualified_name;
        construct.full_name = namespace_path.empty() ? construct.name : namespace_path + "::" + construct.name;
      }
    } else {
      CLILogger::debug("extractFunction: No qualified_identifier found, using fallback");
      // Try to extract name from the full declarator text as fallback
      std::string declarator_text = getNodeText(declarator, content);

      CLILogger::debug("Fallback: extracting from declarator_text: '" + declarator_text + "' for node in " + filename);

      // Special handling for operator functions that might not have text
      if (declarator_text.empty() || declarator_text == "()") {
        // Try to get the text of the whole function_definition node
        std::string full_text = getNodeText(node, content);
        CLILogger::debug("Fallback: empty declarator, trying full node text: '" + full_text.substr(0, 100) + "...'");

        // Look for operator keyword in the full text
        size_t op_pos = full_text.find("operator");
        if (op_pos != std::string::npos) {
          // Extract the operator declaration up to the opening parenthesis
          size_t paren_pos = full_text.find('(', op_pos);
          if (paren_pos != std::string::npos) {
            std::string op_decl = full_text.substr(op_pos, paren_pos - op_pos);
            // Trim whitespace
            while (!op_decl.empty() && std::isspace(op_decl.back())) {
              op_decl.pop_back();
            }
            construct.name = op_decl;
            CLILogger::debug("Fallback: extracted operator from full text: '" + construct.name + "'");

            // If we're inside a class, try to infer the class name from the AST context
            if (!namespace_path.empty() && construct.name.find("::") == std::string::npos) {
              // The namespace_path might already contain the class name for methods
              construct.full_name = namespace_path + "::" + construct.name;
              CLILogger::debug("Fallback: inferred qualified name from context: '" + construct.full_name + "'");
            }
          }
        }
      } else {
        construct.name = extractFunctionNameFromText(declarator_text);
        CLILogger::debug("Fallback: extracted name: '" + construct.name + "'");
      }

      if (construct.name.empty()) {
        // Last resort: try simple identifier
        TSNode simple_name_node = findChildByType(declarator, "identifier");
        if (!ts_node_is_null(simple_name_node)) {
          construct.name = getNodeText(simple_name_node, content);
        }
      }

      // Handle qualified names in the extracted name
      if (construct.name.find("::") != std::string::npos) {
        size_t scope_pos = construct.name.rfind("::");
        construct.namespace_path = construct.name.substr(0, scope_pos);
        std::string method_name = construct.name.substr(scope_pos + 2);
        construct.name = method_name;
        construct.full_name = construct.namespace_path + "::" + construct.name;
      } else {
        construct.full_name = namespace_path.empty() ? construct.name : namespace_path + "::" + construct.name;
      }
    }
  } else {
    CLILogger::debug("extractFunction: No function_declarator found in function_definition");

    // Fallback for inline class methods where Tree-sitter doesn't generate function_declarator nodes
    // This commonly happens with operator overloads and inline method definitions within class bodies
    // Extract method/operator name directly from the full function text as last resort
    std::string func_text = getNodeText(node, content);

    // First check for operators
    size_t op_pos = func_text.find("operator");
    if (op_pos != std::string::npos) {
      // Extract operator declaration up to the opening parenthesis
      size_t paren_pos = func_text.find('(', op_pos);
      if (paren_pos != std::string::npos) {
        std::string op_name = func_text.substr(op_pos, paren_pos - op_pos);
        // Trim whitespace
        while (!op_name.empty() && std::isspace(op_name.back())) {
          op_name.pop_back();
        }
        construct.name = op_name;
        construct.full_name = namespace_path.empty() ? construct.name : namespace_path + "::" + construct.name;
        CLILogger::debug("extractFunction: Extracted operator from inline method: '" + construct.name + "'");
      }
    } else {
      // Try to extract regular method name (e.g., "ClassName::methodName()" or just "methodName()")
      size_t paren_pos = func_text.find('(');
      if (paren_pos != std::string::npos) {
        // Look backwards from the parenthesis to find the method name
        size_t name_end = paren_pos;
        // Skip whitespace before parenthesis
        while (name_end > 0 && std::isspace(func_text[name_end - 1])) {
          name_end--;
        }

        // Find the start of the method name (after :: or whitespace or beginning)
        size_t name_start = name_end;
        while (name_start > 0 &&
               (std::isalnum(func_text[name_start - 1]) ||
                func_text[name_start - 1] == '_' ||
                func_text[name_start - 1] == '~')) {
          name_start--;
        }

        if (name_start < name_end) {
          std::string method_name = func_text.substr(name_start, name_end - name_start);

          // Check if we have a qualified name (Class::method)
          size_t full_start = name_start;
          if (full_start >= 2 && func_text.substr(full_start - 2, 2) == "::") {
            // Find the class name
            size_t class_start = full_start - 2;
            while (class_start > 0 &&
                   (std::isalnum(func_text[class_start - 1]) || func_text[class_start - 1] == '_')) {
              class_start--;
            }
            std::string full_name = func_text.substr(class_start, name_end - class_start);

            size_t scope_pos = full_name.rfind("::");
            if (scope_pos != std::string::npos) {
              construct.namespace_path = full_name.substr(0, scope_pos);
              construct.name = full_name.substr(scope_pos + 2);
              construct.full_name = full_name;
              CLILogger::debug("extractFunction: Extracted qualified method: '" + construct.full_name + "'");
            }
          } else {
            construct.name = method_name;
            construct.full_name = namespace_path.empty() ? construct.name : namespace_path + "::" + construct.name;
            CLILogger::debug("extractFunction: Extracted inline method: '" + construct.name + "'");
          }
        }
      }
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

CodeConstruct ASTExtractor::extractMethodDeclaration(TSNode node, const std::string& content, const std::string& filename, const std::string& namespace_path) {
  CodeConstruct construct;
  construct.type = ConstructType::Function;  // Method declarations are functions
  construct.ast_node = node;
  construct.filename = filename;
  construct.namespace_path = namespace_path;

  CLILogger::debug("extractMethodDeclaration called for node in " + filename + ", namespace: " + namespace_path);

  // Get method name - search for identifier in various possible locations
  TSNode name_node = findChildByType(node, "identifier");
  if (!ts_node_is_null(name_node)) {
    construct.name = getNodeText(name_node, content);
    CLILogger::debug("Method: Found identifier: '" + construct.name + "'");
  } else {
    // Handle destructors (~ClassName) and operators
    TSNode destructor_node = findChildByType(node, "destructor_name");
    if (!ts_node_is_null(destructor_node)) {
      construct.name = getNodeText(destructor_node, content);
      CLILogger::debug("Method: Found destructor: '" + construct.name + "'");
    } else {
      // Fallback: search recursively through all children for identifiers
      construct.name = findMethodName(node, content);
      CLILogger::debug("Method: Using findMethodName, got: '" + construct.name + "'");
    }
  }

  construct.full_name = namespace_path.empty() ? construct.name : namespace_path + "::" + construct.name;

  // Extract return type from parent declaration
  TSNode parent = ts_node_parent(node);
  if (!ts_node_is_null(parent)) {
    construct.return_type = extractReturnType(parent, content);
  }

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

std::string ASTExtractor::findMethodName(TSNode node, const std::string& content) {
  // Extract the full text of the function declarator and parse it manually
  std::string full_text = getNodeText(node, content);

  // Look for pattern: methodName(parameters) or ~methodName() or operator...()
  size_t paren_pos = full_text.find('(');
  if (paren_pos == std::string::npos) return "";

  std::string before_paren = full_text.substr(0, paren_pos);

  // Handle destructor
  if (before_paren.find('~') != std::string::npos) {
    size_t tilde_pos = before_paren.find('~');
    return before_paren.substr(tilde_pos);
  }

  // Handle operators
  if (before_paren.find("operator") != std::string::npos) {
    size_t op_pos = before_paren.find("operator");
    return before_paren.substr(op_pos);
  }

  // Regular method - find the last identifier before (
  // First, trim any whitespace
  while (!before_paren.empty() && std::isspace(before_paren.back())) {
    before_paren.pop_back();
  }

  if (before_paren.empty()) return "";

  // Find the start of the last identifier
  size_t end_pos = before_paren.length();
  size_t start_pos = end_pos;

  // Move backwards while we have identifier characters
  while (start_pos > 0 && (std::isalnum(before_paren[start_pos - 1]) || before_paren[start_pos - 1] == '_')) {
    start_pos--;
  }

  if (start_pos < end_pos) {
    return before_paren.substr(start_pos, end_pos - start_pos);
  }

  return "";
}

std::string ASTExtractor::extractFunctionNameFromText(const std::string& declarator_text) {
  // Extract function name from text like "JsonDoc::operator=(JsonDoc&& other)" or "someFunction(int a, int b)"
  // But also handle cases where we only have "operator=(JsonDoc&& other)" without the class qualification
  size_t paren_pos = declarator_text.find('(');
  if (paren_pos == std::string::npos) {
    return "";
  }

  std::string before_paren = declarator_text.substr(0, paren_pos);

  // Trim whitespace from the end
  while (!before_paren.empty() && std::isspace(before_paren.back())) {
    before_paren.pop_back();
  }

  // Handle qualified names like "JsonDoc::operator="
  if (before_paren.find("::") != std::string::npos) {
    return before_paren;  // Return the full qualified name
  }

  // Handle operators like "operator=", "operator[]", "operator()", etc.
  // This is critical for unqualified operators
  size_t op_pos = before_paren.find("operator");
  if (op_pos != std::string::npos) {
    // Extract the operator including its symbol(s)
    std::string op_name = before_paren.substr(op_pos);
    // Ensure we got something meaningful
    if (!op_name.empty() && op_name != "operator") {
      return op_name;
    }
    // If we just got "operator" without a symbol, try to extract the symbol
    if (op_name == "operator" && op_pos + 8 < before_paren.length()) {
      // Skip "operator" and any spaces
      size_t symbol_start = op_pos + 8;
      while (symbol_start < before_paren.length() && std::isspace(before_paren[symbol_start])) {
        symbol_start++;
      }
      if (symbol_start < before_paren.length()) {
        return "operator" + before_paren.substr(symbol_start);
      }
    }
  }

  // For simple names, extract the last identifier
  size_t pos = before_paren.length();
  size_t start_pos = pos;
  while (start_pos > 0 && (std::isalnum(before_paren[start_pos - 1]) ||
                           before_paren[start_pos - 1] == '_' ||
                           before_paren[start_pos - 1] == '~')) {
    start_pos--;
  }

  if (start_pos < pos) {
    return before_paren.substr(start_pos, pos - start_pos);
  }

  return "";
}

std::string ASTExtractor::escapeSymbolsForFilename(const std::string& name) {
  std::string result = name;

  // Character-level escaping for invalid filename characters
  // Windows forbidden: < > : " | ? * \ /
  // Unix forbidden: / \0
  std::map<char, std::string> escapes = {
    {'<', "%lt"},
    {'>', "%gt"},
    {':', "%colon"},
    {'"', "%quote"},
    {'|', "%pipe"},
    {'?', "%quest"},
    {'*', "%star"},
    {'\\', "%bslash"},
    {'/', "%slash"}
  };

  for (const auto& [ch, replacement] : escapes) {
    size_t pos = 0;
    while ((pos = result.find(ch, pos)) != std::string::npos) {
      result.replace(pos, 1, replacement);
      pos += replacement.length();
    }
  }

  return result;
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

// Docstring merging implementation

int ASTExtractor::mergeDuplicateConstructs(std::vector<CodeConstruct>& constructs) {
  if (constructs.empty()) return 0;

  std::map<std::string, std::vector<size_t>> duplicates;

  // Group constructs by full_name to find duplicates
  for (size_t i = 0; i < constructs.size(); i++) {
    const auto& construct = constructs[i];
    if (!construct.full_name.empty()) {
      duplicates[construct.full_name].push_back(i);
    }
  }

  int conflict_count = 0;
  std::vector<CodeConstruct> merged_constructs;
  std::set<size_t> processed_indices;

  for (const auto& [full_name, indices] : duplicates) {
    if (indices.size() > 1) {
      // Found duplicates - merge them
      CodeConstruct merged = constructs[indices[0]]; // Start with first occurrence
      merged.is_merged = true;
      merged.source_locations.clear();
      merged.merged_docstrings.clear();

      for (size_t idx : indices) {
        processed_indices.insert(idx);
        const auto& construct = constructs[idx];

        // Add location info
        std::string location = construct.filename + ":" + std::to_string(construct.start_line);
        merged.source_locations.push_back(location);

        // Collect docstrings
        if (construct.docstring.has_value() && !construct.docstring->empty()) {
          merged.merged_docstrings.push_back(construct.docstring.value());
        }

        // Detect conflicts between constructs
        if (idx != indices[0]) {
          auto conflicts = detectDocstringConflicts(merged, construct);
          conflict_count += conflicts.size();

          for (const auto& conflict : conflicts) {
            CLILogger::warning("Docstring conflict in " + full_name + ": " + conflict);
          }
        }
      }

      // Merge the docstrings intelligently
      if (!merged.merged_docstrings.empty()) {
        // For now, concatenate all docstrings with separators
        // TODO: Implement smarter merging logic
        std::string combined_docstring;
        for (size_t i = 0; i < merged.merged_docstrings.size(); i++) {
          if (i > 0) combined_docstring += "\n\n";
          combined_docstring += merged.merged_docstrings[i];
        }
        merged.docstring = combined_docstring;
      }

      merged_constructs.push_back(merged);
    } else {
      // Single occurrence - add to processed list
      processed_indices.insert(indices[0]);
      merged_constructs.push_back(constructs[indices[0]]);
    }
  }

  // Add any constructs without full_name (shouldn't happen but safe fallback)
  for (size_t i = 0; i < constructs.size(); i++) {
    if (processed_indices.find(i) == processed_indices.end()) {
      merged_constructs.push_back(constructs[i]);
    }
  }

  constructs = std::move(merged_constructs);
  return conflict_count;
}

CodeConstruct ASTExtractor::mergeConstructs(const CodeConstruct& declaration_construct, const CodeConstruct& implementation_construct) {
  CodeConstruct merged = declaration_construct; // Start with declaration
  merged.is_merged = true;

  // Merge location information
  merged.source_locations = {
    declaration_construct.filename + ":" + std::to_string(declaration_construct.start_line),
    implementation_construct.filename + ":" + std::to_string(implementation_construct.start_line)
  };

  // Collect docstrings
  merged.merged_docstrings.clear();
  if (declaration_construct.docstring.has_value() && !declaration_construct.docstring->empty()) {
    merged.merged_docstrings.push_back(declaration_construct.docstring.value());
  }
  if (implementation_construct.docstring.has_value() && !implementation_construct.docstring->empty()) {
    merged.merged_docstrings.push_back(implementation_construct.docstring.value());
  }

  // Combine docstrings
  if (!merged.merged_docstrings.empty()) {
    std::string combined_docstring;
    for (size_t i = 0; i < merged.merged_docstrings.size(); i++) {
      if (i > 0) combined_docstring += "\n\n";
      combined_docstring += merged.merged_docstrings[i];
    }
    merged.docstring = combined_docstring;
  }

  return merged;
}

std::vector<std::string> ASTExtractor::detectDocstringConflicts(const CodeConstruct& construct1, const CodeConstruct& construct2) {
  std::vector<std::string> conflicts;

  // For now, implement basic parameter conflict detection
  // TODO: Parse docstring content to detect @param conflicts

  // Check if both have docstrings but they're different
  if (construct1.docstring.has_value() && construct2.docstring.has_value() &&
      !construct1.docstring->empty() && !construct2.docstring->empty()) {

    if (construct1.docstring.value() != construct2.docstring.value()) {
      conflicts.push_back("Different docstring content in " + construct1.filename + " vs " + construct2.filename);
    }
  }

  // Check parameter count mismatch
  if (construct1.parameters.size() != construct2.parameters.size()) {
    conflicts.push_back("Parameter count mismatch: " + std::to_string(construct1.parameters.size()) +
                       " vs " + std::to_string(construct2.parameters.size()));
  }

  return conflicts;
}
