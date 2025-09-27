/**
 * @file javadoc.cpp
 * @brief Javadoc-style documentation block parsing implementation
 */
#include "javadoc.h"
#include <sstream>
#include <regex>

std::vector<JavadocBlock> JavadocParser::extractJavadocBlocks(const std::string& content, const std::string& style) {
  if (style == "/** */") {
    return extractBlockComments(content);
  } else if (style == "/// ") {
    return extractLineComments(content, "///");
  } else if (style == "//! ") {
    return extractLineComments(content, "//!");
  }
  return {};
}

std::vector<JavadocBlock> JavadocParser::extractBlockComments(const std::string& content) {
  std::vector<JavadocBlock> blocks;
  
  try {
    std::regex pattern(R"(/\*\*[\s\S]*?\*/)");
    auto begin = std::sregex_iterator(content.begin(), content.end(), pattern);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
      const std::smatch& match = *it;
      JavadocBlock block = parseJavadocContent(match.str());
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

std::vector<JavadocBlock> JavadocParser::extractBlockCommentsSimple(const std::string& content) {
  std::vector<JavadocBlock> blocks;
  size_t pos = 0;
  
  while (pos < content.length()) {
    size_t start = content.find("/**", pos);
    if (start == std::string::npos) break;
    
    size_t end = content.find("*/", start + 3);
    if (end == std::string::npos) break;
    
    end += 2; // Include the */
    
    std::string comment = content.substr(start, end - start);
    JavadocBlock block = parseJavadocContent(comment);
    block.location = getSourceLocation(content, start);
    blocks.push_back(block);
    
    pos = end;
  }
  
  return blocks;
}

std::vector<JavadocBlock> JavadocParser::extractLineComments(const std::string& content, const std::string& prefix) {
  std::vector<JavadocBlock> blocks;
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
      JavadocBlock block = parseJavadocContent("/**" + current_comment + "*/");
      block.location = comment_start;
      blocks.push_back(block);
      in_comment = false;
    }

    byte_offset += line.length() + 1; // +1 for newline
  }

  // Handle comment at end of file
  if (in_comment) {
    JavadocBlock block = parseJavadocContent("/**" + current_comment + "*/");
    block.location = comment_start;
    blocks.push_back(block);
  }

  return blocks;
}

JavadocBlock JavadocParser::parseJavadocContent(const std::string& raw) {
  JavadocBlock block;
  block.raw_content = raw;

  std::string cleaned = cleanJavadocContent(raw);

  std::regex param_regex(R"(@param\s+(\w+)\s+(.+))");
  std::regex return_regex(R"(@return\s+(.+))");
  std::regex tag_regex(R"(@(\w+)(?:\s+(.+))?)");

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
    } else if (std::regex_search(line, match, tag_regex)) {
      block.tags.push_back(match[1].str() + (match[2].matched ? ": " + match[2].str() : ""));
      in_description = false;
    } else if (in_description && !line.empty()) {
      description += line + "\n";
    }
  }

  block.description = trim(description);
  return block;
}

std::string JavadocParser::cleanJavadocContent(const std::string& raw) {
  std::string result = raw;

  // Remove /** and */
  if (result.length() >= 4) {
    result = result.substr(3, result.length() - 5);
  }

  // Remove leading * from each line
  std::istringstream ss(result);
  std::string line;
  std::ostringstream cleaned;

  while (std::getline(ss, line)) {
    // Remove leading whitespace and *
    size_t start = line.find_first_not_of(" \t");
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

std::string JavadocParser::trim(const std::string& str) {
  size_t start = str.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";

  size_t end = str.find_last_not_of(" \t\n\r");
  return str.substr(start, end - start + 1);
}

SourceLocation JavadocParser::getSourceLocation(const std::string& content, size_t byte_offset) {
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