/**
 * @file markdowngen.cpp
 * @brief Markdown documentation file generation implementation
 */
#include "markdowngen.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

void MarkdownGenerator::generateMarkdownFiles(const std::vector<JavadocBlock>& blocks,
                                              const std::string& output_dir) {
  std::filesystem::create_directories(output_dir);

  for (const auto& block : blocks) {
    if (!block.symbol_name.empty()) {
      std::string filename = generateFilename(block);
      std::string filepath = output_dir + "/" + filename;
      generateMarkdownFile(block, filepath);
    }
  }
}

std::string MarkdownGenerator::generateFilename(const JavadocBlock& block) {
  // Create filename based on namespace and symbol name
  std::string name = block.namespace_path;
  if (!name.empty() && !block.symbol_name.empty()) {
    name += "::" + block.symbol_name;
  } else if (!block.symbol_name.empty()) {
    name = block.symbol_name;
  } else {
    name = "unnamed";
  }

  // Replace :: with - and make filesystem safe
  std::replace(name.begin(), name.end(), ':', '-');
  std::replace(name.begin(), name.end(), ' ', '_');

  return name + ".md";
}

void MarkdownGenerator::generateMarkdownFile(const JavadocBlock& block, const std::string& filepath) {
  std::ofstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "Failed to create file: " << filepath << std::endl;
    return;
  }

  // YAML frontmatter
  file << "---\n";
  file << "type: " << block.symbol_type << "\n";
  if (!block.namespace_path.empty()) {
    file << "namespace: " << block.namespace_path << "\n";
  }
  file << "name: " << block.symbol_name << "\n";
  file << "line: " << block.location.line << "\n";
  if (!block.namespace_path.empty()) {
    // Extract parent namespace
    size_t last_sep = block.namespace_path.find_last_of(":");
    if (last_sep != std::string::npos && last_sep >= 2) {
      // Find the actual :: separator, not just the last :
      if (last_sep > 0 && block.namespace_path[last_sep-1] == ':') {
        file << "parent: " << block.namespace_path.substr(0, last_sep - 1) << "\n";
      }
    }
  }
  file << "---\n\n";

  // Content
  file << "# " << block.symbol_name << "\n\n";

  if (!block.description.empty()) {
    file << block.description << "\n\n";
  }

  // Parameters
  if (!block.params.empty()) {
    file << "## Parameters\n\n";
    for (const auto& [param_name, param_desc] : block.params) {
      file << "- **" << param_name << "**: " << param_desc << "\n";
    }
    file << "\n";
  }

  // Return value
  if (!block.return_desc.empty()) {
    file << "## Returns\n\n";
    file << block.return_desc << "\n";
  }

  // Additional tags
  if (!block.tags.empty()) {
    file << "## Additional Information\n\n";
    for (const auto& tag : block.tags) {
      file << "- " << tag << "\n";
    }
  }

  file.close();
}

void MarkdownGenerator::generateMarkdownFromConstructs(const std::vector<CodeConstruct>& constructs,
                                                       const std::string& output_dir) {
  std::filesystem::create_directories(output_dir);

  for (const auto& construct : constructs) {
    std::string filename = generateConstructFilename(construct);
    std::string filepath = output_dir + "/" + filename;
    generateConstructMarkdownFile(construct, filepath);
    std::cout << "Generated: " << filename << std::endl;
  }
}

std::string MarkdownGenerator::generateConstructFilename(const CodeConstruct& construct) {
  std::string name = construct.full_name;
  if (name.empty()) {
    name = construct.name;
  }
  if (name.empty()) {
    name = "unnamed_" + formatConstructType(construct.type);
  }

  // Replace :: with - and make filesystem safe
  std::replace(name.begin(), name.end(), ':', '-');
  std::replace(name.begin(), name.end(), ' ', '_');
  std::replace(name.begin(), name.end(), '<', '[');
  std::replace(name.begin(), name.end(), '>', ']');

  return name + ".md";
}

void MarkdownGenerator::generateConstructMarkdownFile(const CodeConstruct& construct, const std::string& filepath) {
  std::ofstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "Failed to create file: " << filepath << std::endl;
    return;
  }

  // YAML frontmatter
  file << "---\n";
  file << "type: " << formatConstructType(construct.type) << "\n";
  if (!construct.namespace_path.empty()) {
    file << "namespace: " << construct.namespace_path << "\n";
  }
  file << "name: " << construct.name << "\n";
  file << "full_name: " << construct.full_name << "\n";
  file << "start_line: " << construct.start_line << "\n";
  file << "end_line: " << construct.end_line << "\n";
  file << "file: " << construct.filename << "\n";
  if (construct.return_type.has_value()) {
    file << "return_type: " << construct.return_type.value() << "\n";
  }
  file << "---\n\n";

  // Title
  file << "# " << construct.name << "\n\n";

  // Type and signature
  file << "*" << formatConstructType(construct.type);
  if (!construct.namespace_path.empty()) {
    file << " in " << construct.namespace_path;
  }
  file << "*\n\n";

  // Function signature
  if (construct.type == ConstructType::Function || construct.type == ConstructType::Method) {
    file << "## Signature\n\n";
    file << "```cpp\n";
    file << formatFunctionSignature(construct);
    file << "\n```\n\n";
  }

  // Parameters
  if (!construct.parameters.empty()) {
    file << "## Parameters\n\n";
    file << "| Name | Type | Description |\n";
    file << "|------|------|-------------|\n";
    for (const auto& param : construct.parameters) {
      file << "| `" << param.name << "` | `" << param.type << "` | ";
      if (param.default_value.has_value()) {
        file << "*Default: `" << param.default_value.value() << "`*";
      } else {
        file << "*(No description available)*";
      }
      file << " |\n";
    }
    file << "\n";
  }

  // Return type
  if (construct.return_type.has_value() && construct.return_type.value() != "void") {
    file << "## Returns\n\n";
    file << "`" << construct.return_type.value() << "`\n\n";
    file << "*(No description available)*\n\n";
  }

  // Documentation from docstring
  if (construct.docstring.has_value()) {
    file << "## Documentation\n\n";
    file << construct.docstring.value() << "\n\n";
  } else {
    file << "## Documentation\n\n";
    file << "*No documentation available. This " << formatConstructType(construct.type) 
         << " was automatically discovered from the source code.*\n\n";
  }

  // File location
  file << "## Source\n\n";
  file << "**File:** `" << construct.filename << "`\n\n";
  file << "**Lines:** " << construct.start_line << "-" << construct.end_line << "\n";

  file.close();
}

std::string MarkdownGenerator::formatConstructType(ConstructType type) {
  switch (type) {
    case ConstructType::Function: return "function";
    case ConstructType::Method: return "method";
    case ConstructType::Class: return "class";
    case ConstructType::Struct: return "struct";
    case ConstructType::Enum: return "enum";
    case ConstructType::Variable: return "variable";
    case ConstructType::Namespace: return "namespace";
    case ConstructType::Constructor: return "constructor";
    case ConstructType::Destructor: return "destructor";
    default: return "unknown";
  }
}

std::string MarkdownGenerator::formatFunctionSignature(const CodeConstruct& construct) {
  std::string signature;
  
  // Return type
  if (construct.return_type.has_value()) {
    signature += construct.return_type.value() + " ";
  }
  
  // Function name
  signature += construct.name;
  
  // Parameters
  signature += "(";
  signature += formatParameters(construct.parameters);
  signature += ")";
  
  // Qualifiers
  if (construct.is_const) {
    signature += " const";
  }
  
  return signature;
}

std::string MarkdownGenerator::formatParameters(const std::vector<Parameter>& parameters) {
  if (parameters.empty()) {
    return "";
  }
  
  std::string result;
  for (size_t i = 0; i < parameters.size(); i++) {
    if (i > 0) {
      result += ", ";
    }
    
    result += parameters[i].type;
    if (!parameters[i].name.empty()) {
      result += " " + parameters[i].name;
    }
    
    if (parameters[i].default_value.has_value()) {
      result += " = " + parameters[i].default_value.value();
    }
  }
  
  return result;
}
