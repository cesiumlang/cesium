/**
@brief Tree-sitter AST parsing utilities implementation
*/
#include <backend/doc/cpp/ts_ast_parser.h>
// #include <cstring>

void DocAssociator::associateDocsWithNodes(std::vector<DocstringBlock>& docstring_blocks,
                                           TSTree* tree, const std::string& content) {
  TSNode root = ts_tree_root_node(tree);

  for (auto& block : docstring_blocks) {
    TSNode following_node = findFollowingDeclaration(tree, root, block.location);
    if (!ts_node_is_null(following_node)) {
      block.associated_node = following_node;
      block.namespace_path = extractNamespacePath(following_node, content);
      block.symbol_name = extractSymbolName(following_node, content);
      block.symbol_type = std::string(ts_node_type(following_node));
    }
  }
}

TSNode DocAssociator::findFollowingDeclaration(TSTree* tree, TSNode root, SourceLocation docstring_loc) {
                                               //const std::string& content) {
  const char* query_string = R"(
    [
      (function_definition) @decl
      (class_specifier) @decl
      (namespace_definition) @decl
      (struct_specifier) @decl
      (enum_specifier) @decl
    ]
  )";

  const TSLanguage* language = ts_tree_language(tree);
  uint32_t error_offset;
  TSQueryError error_type;
  TSQuery* query = ts_query_new(language, query_string, strlen(query_string),
                               &error_offset, &error_type);

  if (!query) return {};

  TSQueryCursor* cursor = ts_query_cursor_new();
  ts_query_cursor_exec(cursor, query, root);

  TSQueryMatch match;
  TSNode best_match = {};
  uint32_t best_distance = UINT32_MAX;

  while (ts_query_cursor_next_match(cursor, &match)) {
    TSNode node = match.captures[0].node;
    uint32_t start_byte = ts_node_start_byte(node);

    if (start_byte > docstring_loc.byte_offset) {
      uint32_t distance = start_byte - docstring_loc.byte_offset;
      if (distance < best_distance) {
        best_distance = distance;
        best_match = node;
      }
    }
  }

  ts_query_cursor_delete(cursor);
  ts_query_delete(query);
  return best_match;
}

std::string DocAssociator::extractNamespacePath(TSNode node, const std::string& content) {
  std::vector<std::string> path_parts;
  TSNode current = node;

  while (!ts_node_is_null(current)) {
    std::string node_type = ts_node_type(current);

    if (node_type == "namespace_definition" || node_type == "class_specifier") {
      TSNode name_node = getNameNode(current);
      if (!ts_node_is_null(name_node)) {
        std::string name = getNodeText(name_node, content);
        path_parts.insert(path_parts.begin(), name);
      }
    }

    current = ts_node_parent(current);
  }

  return joinPath(path_parts, "::");
}

std::string DocAssociator::extractSymbolName(TSNode node, const std::string& content) {
  TSNode name_node = getNameNode(node);
  return ts_node_is_null(name_node) ? "" : getNodeText(name_node, content);
}

TSNode DocAssociator::getNameNode(TSNode node) {
  std::string node_type = ts_node_type(node);

  // Special handling for function definitions
  if (node_type == "function_definition") {
    // Look for function_declarator child, then name within it
    for (uint32_t i = 0; i < ts_node_child_count(node); i++) {
      TSNode child = ts_node_child(node, i);
      if (std::string(ts_node_type(child)) == "function_declarator") {
        // Look for qualified_identifier first (for operators and qualified names)
        for (uint32_t j = 0; j < ts_node_child_count(child); j++) {
          TSNode grandchild = ts_node_child(child, j);
          std::string grandchild_type = ts_node_type(grandchild);
          if (grandchild_type == "qualified_identifier") {
            return grandchild;
          }
        }
        // Fall back to simple identifier if no qualified_identifier found
        for (uint32_t j = 0; j < ts_node_child_count(child); j++) {
          TSNode grandchild = ts_node_child(child, j);
          if (std::string(ts_node_type(grandchild)) == "identifier") {
            return grandchild;
          }
        }
      }
    }
  }

  // Default behavior for other node types
  for (uint32_t i = 0; i < ts_node_child_count(node); i++) {
    TSNode child = ts_node_child(node, i);
    std::string type = ts_node_type(child);
    if (type == "identifier" || type == "type_identifier") {
      return child;
    }
  }
  return {};
}

std::string DocAssociator::getNodeText(TSNode node, const std::string& content) {
  uint32_t start = ts_node_start_byte(node);
  uint32_t end = ts_node_end_byte(node);
  if (end > content.length()) end = content.length();
  return content.substr(start, end - start);
}

std::string DocAssociator::joinPath(const std::vector<std::string>& parts, const std::string& separator) {
  if (parts.empty()) return "";

  std::string result = parts[0];
  for (size_t i = 1; i < parts.size(); i++) {
    result += separator + parts[i];
  }
  return result;
}
