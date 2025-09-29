/**
@brief Markdown documentation file generation implementation
*/
#include <backend/doc/markdowngen.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <map>
#include <backend/core/cli_utils.h>

// Local utility function for escaping symbols in filenames
std::string escapeSymbolsForFilename(const std::string& name) {
  std::string result = name;
  
  // Character-level escaping for invalid filename characters
  // Note: : is not escaped here since :: is handled separately by converting to .
  std::map<char, std::string> escapes = {
    {'<', "%lt"},
    {'>', "%gt"},
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

void MarkdownGenerator::generateMarkdownFiles(const std::vector<DocstringBlock>& blocks,
                                              const std::string& output_dir) {
  CLILogger::debug("MarkdownGenerator::generateMarkdownFiles: Starting generation for " + std::to_string(blocks.size()) + " blocks to directory: " + output_dir);
  
  try {
    std::filesystem::create_directories(output_dir);
    CLILogger::debug("MarkdownGenerator::generateMarkdownFiles: Successfully created output directory: " + output_dir);
  } catch (const std::exception& e) {
    CLILogger::error("MarkdownGenerator::generateMarkdownFiles: Failed to create output directory " + output_dir + ": " + e.what());
    return;
  }

  int files_generated = 0;
  int blocks_skipped = 0;
  
  for (const auto& block : blocks) {
    if (!block.symbol_name.empty()) {
      std::string filename = generateFilename(block);
      std::string filepath = output_dir + "/" + filename;
      CLILogger::debug("MarkdownGenerator::generateMarkdownFiles: Generating file for symbol '" + block.symbol_name + "' -> " + filename);
      generateMarkdownFile(block, filepath);
      files_generated++;
    } else {
      blocks_skipped++;
      CLILogger::debug("MarkdownGenerator::generateMarkdownFiles: Skipping block with empty symbol_name (line " + std::to_string(block.location.line) + ")");
    }
  }
  
  CLILogger::debug("MarkdownGenerator::generateMarkdownFiles: Completed generation - " + std::to_string(files_generated) + " files generated, " + std::to_string(blocks_skipped) + " blocks skipped");
}

std::string MarkdownGenerator::generateFilename(const DocstringBlock& block) {
  CLILogger::debug("MarkdownGenerator::generateFilename: Generating filename for symbol '" + block.symbol_name + "' in namespace '" + block.namespace_path + "'");
  
  // Create filename based on namespace and symbol name
  std::string name = block.namespace_path;
  if (!name.empty() && !block.symbol_name.empty()) {
    name += "::" + block.symbol_name;
  } else if (!block.symbol_name.empty()) {
    name = block.symbol_name;
  } else {
    name = "unnamed";
    CLILogger::warning("MarkdownGenerator::generateFilename: Block has empty symbol_name, using 'unnamed'");
  }
  
  CLILogger::debug("MarkdownGenerator::generateFilename: Raw name before sanitization: '" + name + "'");

  // Replace :: with - and make filesystem safe
  std::replace(name.begin(), name.end(), ':', '-');
  std::replace(name.begin(), name.end(), ' ', '_');
  
  std::string filename = name + ".md";
  CLILogger::debug("MarkdownGenerator::generateFilename: Final filename: '" + filename + "'");
  return filename;
}

void MarkdownGenerator::generateMarkdownFile(const DocstringBlock& block, const std::string& filepath) {
  CLILogger::debug("MarkdownGenerator::generateMarkdownFile: Creating markdown file: " + filepath);
  
  std::ofstream file(filepath);
  if (!file.is_open()) {
    CLILogger::error("MarkdownGenerator::generateMarkdownFile: Failed to create file: " + filepath);
    return;
  }
  
  CLILogger::debug("MarkdownGenerator::generateMarkdownFile: Successfully opened file for writing: " + filepath);

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
  
  if (!file.good()) {
    CLILogger::error("MarkdownGenerator::generateMarkdownFile: File write errors occurred while generating " + filepath);
  } else {
    CLILogger::debug("MarkdownGenerator::generateMarkdownFile: Successfully completed file: " + filepath);
  }

  file.close();
}

std::vector<std::string> MarkdownGenerator::generateMarkdownFromConstructs(const std::vector<CodeConstruct>& constructs,
                                                                          const std::string& output_dir) {
  CLILogger::debug("MarkdownGenerator::generateMarkdownFromConstructs: Starting generation for " + std::to_string(constructs.size()) + " constructs to directory: " + output_dir);
  
  try {
    std::filesystem::create_directories(output_dir);
    CLILogger::debug("MarkdownGenerator::generateMarkdownFromConstructs: Successfully created output directory: " + output_dir);
  } catch (const std::exception& e) {
    CLILogger::error("MarkdownGenerator::generateMarkdownFromConstructs: Failed to create output directory " + output_dir + ": " + e.what());
    return {};
  }
  std::vector<std::string> generated_files;
  int successful_generations = 0;
  int failed_generations = 0;

  for (const auto& construct : constructs) {
    std::string filename = generateConstructFilename(construct);
    std::string filepath = output_dir + "/" + filename;
    CLILogger::debug("MarkdownGenerator::generateMarkdownFromConstructs: Processing construct '" + construct.full_name + "' -> " + filename);
    
    try {
      generateConstructMarkdownFile(construct, filepath);
      generated_files.push_back(filepath);
      successful_generations++;
      std::cout << "Generated: " << filename << std::endl;
    } catch (const std::exception& e) {
      CLILogger::error("MarkdownGenerator::generateMarkdownFromConstructs: Failed to generate file for construct '" + construct.full_name + "': " + e.what());
      failed_generations++;
    }
  }
  
  CLILogger::debug("MarkdownGenerator::generateMarkdownFromConstructs: Completed generation - " + std::to_string(successful_generations) + " successful, " + std::to_string(failed_generations) + " failed");
  return generated_files;
}

std::string MarkdownGenerator::generateConstructFilename(const CodeConstruct& construct) {
  CLILogger::debug("MarkdownGenerator::generateConstructFilename: Generating filename for construct '" + construct.full_name + "' (name: '" + construct.name + "', type: " + formatConstructType(construct.type) + ")");
  
  std::string name = construct.full_name;
  if (name.empty()) {
    name = construct.name;
    CLILogger::debug("MarkdownGenerator::generateConstructFilename: full_name empty, using name: '" + name + "'");
  }
  if (name.empty()) {
    name = "unnamed_" + formatConstructType(construct.type);
    CLILogger::warning("MarkdownGenerator::generateConstructFilename: Both full_name and name empty, using fallback: '" + name + "'");
  }

  // First replace :: with . for dot-delimited fully-qualified names
  std::replace(name.begin(), name.end(), ':', '.');
  // Remove extra dots from double-colon conversion
  size_t pos = 0;
  while ((pos = name.find("..", pos)) != std::string::npos) {
    name.replace(pos, 2, ".");
    pos += 1;
  }
  
  // Then escape problematic characters (but not : since we already handled ::)
  name = escapeSymbolsForFilename(name);
  
  // Make other characters filesystem safe
  std::replace(name.begin(), name.end(), ' ', '_');

  std::string filename = name + ".md";
  CLILogger::debug("MarkdownGenerator::generateConstructFilename: Final filename: '" + filename + "'");
  return filename;
}

void MarkdownGenerator::generateConstructMarkdownFile(const CodeConstruct& construct, const std::string& filepath) {
  CLILogger::debug("MarkdownGenerator::generateConstructMarkdownFile: Creating markdown file for construct '" + construct.full_name + "' at: " + filepath);
  
  std::ofstream file(filepath);
  if (!file.is_open()) {
    CLILogger::error("MarkdownGenerator::generateConstructMarkdownFile: Failed to create file: " + filepath);
    return;
  }
  
  CLILogger::debug("MarkdownGenerator::generateConstructMarkdownFile: Successfully opened file for writing: " + filepath);

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
  
  // Include merged docstring information
  if (construct.is_merged) {
    file << "is_merged: true\n";
    if (!construct.source_locations.empty()) {
      file << "source_locations:\n";
      for (const auto& location : construct.source_locations) {
        file << "  - " << location << "\n";
      }
    }
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

  if (!file.good()) {
    CLILogger::error("MarkdownGenerator::generateConstructMarkdownFile: File write errors occurred while generating " + filepath);
  } else {
    CLILogger::debug("MarkdownGenerator::generateConstructMarkdownFile: Successfully completed file: " + filepath);
  }

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
