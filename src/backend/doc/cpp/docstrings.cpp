/**
@brief Documentation string parsing implementation
*/
#include <backend/doc/cpp/docstrings.h>
#include <sstream>
#include <regex>
#include <iostream>

std::vector<DocstringBlock> DocstringParser::extractDocstrings(const std::string& content, const std::string& style) {
  if (style == "/** */") {
    return extractBlockComments(content);
  } else if (style == "/// ") {
    return extractLineComments(content, "///");
  } else if (style == "//! ") {
    return extractLineComments(content, "//!");
  }
  return {};
}

std::vector<DocstringBlock> DocstringParser::extractBlockComments(const std::string& content) {
  std::vector<DocstringBlock> blocks;
  
  try {
    std::regex pattern(R"(/\*\*[\s\S]*?\*/)");
    auto begin = std::sregex_iterator(content.begin(), content.end(), pattern);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
      const std::smatch& match = *it;
      DocstringBlock block = parseDocstringContent(match.str());
      block.location = getSourceLocation(content, match.position());
      blocks.push_back(block);
    }
  } catch (const std::regex_error& e) {
    std::cerr << "Regex error in extractBlockComments: " << e.what() << std::endl;
    // Fall back to simpler string-based parsing if regex fails
    return extractBlockCommentsSimple(content);
  }
  
  return blocks;
}

std::vector<DocstringBlock> DocstringParser::extractBlockCommentsSimple(const std::string& content) {
  std::vector<DocstringBlock> blocks;
  size_t pos = 0;
  
  while (pos < content.length()) {
    size_t start = content.find("/**", pos);
    if (start == std::string::npos) break;
    
    size_t end = content.find("*/", start + 3);
    if (end == std::string::npos) break;
    
    end += 2; // Include the */
    
    std::string comment = content.substr(start, end - start);
    DocstringBlock block = parseDocstringContent(comment);
    block.location = getSourceLocation(content, start);
    blocks.push_back(block);
    
    pos = end;
  }
  
  return blocks;
}

std::vector<DocstringBlock> DocstringParser::extractLineComments(const std::string& content, const std::string& prefix) {
  std::vector<DocstringBlock> blocks;
  std::istringstream stream(content);
  std::string line;
  size_t line_num = 0;
  size_t byte_offset = 0;

  std::string current_comment;
  SourceLocation comment_start = {0, 0, 0};
  bool in_comment = false;

  while (std::getline(stream, line)) {
    line_num++;

    if (line.find(prefix) == 0) {
      if (!in_comment) {
        comment_start = {line_num, 0, byte_offset};
        in_comment = true;
        current_comment.clear();
      }
      current_comment += line.substr(prefix.length()) + "\n";
    } else if (in_comment) {
      // End of comment block
      DocstringBlock block = parseDocstringContent("/**" + current_comment + "*/");
      block.location = comment_start;
      blocks.push_back(block);
      in_comment = false;
    }

    byte_offset += line.length() + 1; // +1 for newline
  }

  // Handle comment at end of file
  if (in_comment) {
    DocstringBlock block = parseDocstringContent("/**" + current_comment + "*/");
    block.location = comment_start;
    blocks.push_back(block);
  }

  return blocks;
}

DocstringBlock DocstringParser::parseDocstringContent(const std::string& raw) {
  DocstringBlock block;
  block.raw_content = raw;

  std::string cleaned = cleanDocstringContent(raw);

  // Support both Javadoc (@param) and Doxygen (\param) syntax
  std::regex param_regex(R"([@\\]param\s+(\w+)\s+(.+))");
  std::regex return_regex(R"([@\\]return(?:s)?\s+(.+))");
  std::regex brief_regex(R"([@\\]brief\s+(.+))");
  std::regex file_regex(R"([@\\]file\s+(.+))");
  std::regex class_regex(R"([@\\]class\s+(\w+))");
  std::regex struct_regex(R"([@\\]struct\s+(\w+))");
  std::regex enum_regex(R"([@\\]enum\s+(\w+))");
  std::regex tag_regex(R"([@\\](\w+)(?:\s+(.+))?)");

  std::istringstream ss(cleaned);
  std::string line;
  std::string description;
  bool in_description = true;

  while (std::getline(ss, line)) {
    std::smatch match;

    if (std::regex_search(line, match, param_regex)) {
      block.params[match[1].str()] = trim(match[2].str());
      in_description = false;
    } else if (std::regex_search(line, match, return_regex)) {
      block.return_desc = trim(match[1].str());
      in_description = false;
    } else if (std::regex_search(line, match, brief_regex)) {
      // For Doxygen @brief, this becomes the main description
      if (block.description.empty()) {
        block.description = trim(match[1].str());
      }
      in_description = false;
    } else if (std::regex_search(line, match, file_regex)) {
      // Optional override for filename
      block.override_file = trim(match[1].str());
      in_description = false;
    } else if (std::regex_search(line, match, class_regex)) {
      // Optional override for class name
      block.override_class = trim(match[1].str());
      in_description = false;
    } else if (std::regex_search(line, match, struct_regex)) {
      // Optional override for struct name
      block.override_struct = trim(match[1].str());
      in_description = false;
    } else if (std::regex_search(line, match, enum_regex)) {
      // Optional override for enum name
      block.override_enum = trim(match[1].str());
      in_description = false;
    } else if (std::regex_search(line, match, tag_regex)) {
      // Store other tags for completeness
      std::string tag_name = match[1].str();
      if (tag_name != "file" && tag_name != "class" && tag_name != "struct" && tag_name != "enum") {
        block.tags.push_back(tag_name + (match[2].matched ? ": " + match[2].str() : ""));
      }
      in_description = false;
    } else if (in_description && !line.empty()) {
      description += line + "\n";
    }
  }

  block.description = trim(description);
  return block;
}

std::string DocstringParser::cleanDocstringContent(const std::string& raw) {
  std::string result = raw;

  // Remove /** and */
  if (result.length() >= 4) {
    result = result.substr(3, result.length() - 5);
  }

  // Remove leading * from each line (optional, for compatibility with traditional formats)
  std::istringstream ss(result);
  std::string line;
  std::ostringstream cleaned;

  while (std::getline(ss, line)) {
    // Remove leading whitespace
    size_t start = line.find_first_not_of(" \t");
    
    // If line starts with *, remove it (optional for backward compatibility)
    if (start != std::string::npos && line[start] == '*') {
      start++;
      if (start < line.length() && line[start] == ' ') {
        start++;
      }
    }

    if (start != std::string::npos) {
      cleaned << line.substr(start) << "\n";
    } else {
      cleaned << "\n";
    }
  }

  return cleaned.str();
}

std::string DocstringParser::trim(const std::string& str) {
  size_t start = str.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";

  size_t end = str.find_last_not_of(" \t\n\r");
  return str.substr(start, end - start + 1);
}

SourceLocation DocstringParser::getSourceLocation(const std::string& content, size_t byte_offset) {
  size_t line = 1;
  size_t column = 1;

  for (size_t i = 0; i < byte_offset && i < content.length(); i++) {
    if (content[i] == '\n') {
      line++;
      column = 1;
    } else {
      column++;
    }
  }

  return {line, column, byte_offset};
}