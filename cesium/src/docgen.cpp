/**
 * @file docgen.cpp
 * @brief Core documentation extraction and generation implementation
 */
#include "docgen.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include "tree_sitter/api.h"
#include "json.h"

bool CesiumDocExtractor::initialize(const std::string& config_path) {
  auto config = JsonDoc::fromFile(config_path);
  if (!config) {
    std::cerr << "Failed to load configuration from: " << config_path << std::endl;
    return false;
  }

  // Load language parsers
  const JsonDoc& config_ref = *config;
  JsonValue languages = config_ref["languages"];
  
  languages.forEachObject([&](const std::string& lang_name, JsonValue lang_config) {
    if (loader_.loadLanguage(lang_name, lang_config)) {
      std::cout << "Loaded " << lang_name << " parser" << std::endl;
    } else {
      std::cerr << "Warning: Failed to load " << lang_name << " parser" << std::endl;
    }
  });

  return true;
}

void CesiumDocExtractor::extractDocs(const std::string& config_path) {
  auto config = JsonDoc::fromFile(config_path);
  if (!config) return;

  std::vector<CodeConstruct> all_constructs;

  // Process each source directory
  const JsonDoc& config_ref = *config;
  JsonValue source_dirs_value = config_ref["source_directories"];
  
  source_dirs_value.forEachArray([&](size_t idx, JsonValue dir_value) {
    std::string dir_str = static_cast<std::string>(dir_value);
    std::cout << "Processing directory: " << dir_str << std::endl;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_str)) {
      if (entry.is_regular_file()) {
        std::string filepath = entry.path().string();
        auto [lang_name, lang_info] = loader_.getLanguageForFile(filepath);

        if (lang_info) {
          std::cout << "Processing " << filepath << " as " << lang_name << std::endl;
          auto constructs = extractAllConstructs(filepath, *lang_info);
          all_constructs.insert(all_constructs.end(), constructs.begin(), constructs.end());
        }
      }
    }
  });

  // Generate markdown files from AST constructs
  std::string output_dir = static_cast<std::string>(config_ref["output_directory"]);
  std::cout << "Generating documentation for " << all_constructs.size() << " code constructs in " << output_dir << std::endl;
  markdown_generator_.generateMarkdownFromConstructs(all_constructs, output_dir);
}

std::vector<JavadocBlock> CesiumDocExtractor::extractFromFile(const std::string& filepath,
                                                             const LanguageInfo& lang_info) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filepath << std::endl;
    return {};
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
  file.close();

  // Extract javadoc comments
  auto javadoc_blocks = javadoc_parser_.extractJavadocBlocks(content, lang_info.javadoc_style);

  // Parse with tree-sitter
  TSParser* parser = ts_parser_new();
  ts_parser_set_language(parser, lang_info.language);
  TSTree* tree = ts_parser_parse_string(parser, nullptr, content.c_str(), content.length());

  // Associate comments with AST nodes
  doc_associator_.associateDocsWithNodes(javadoc_blocks, tree, content);

  // Cleanup
  ts_tree_delete(tree);
  ts_parser_delete(parser);

  return javadoc_blocks;
}

std::vector<CodeConstruct> CesiumDocExtractor::extractAllConstructs(const std::string& filepath,
                                                                   const LanguageInfo& lang_info) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filepath << std::endl;
    return {};
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
  file.close();

  // Parse with tree-sitter
  TSParser* parser = ts_parser_new();
  ts_parser_set_language(parser, lang_info.language);
  TSTree* tree = ts_parser_parse_string(parser, nullptr, content.c_str(), content.length());

  // Extract all code constructs from AST
  std::vector<CodeConstruct> constructs = ast_extractor_.extractConstructs(tree, content, filepath);

  // Optional: Extract javadoc comments and associate them with constructs
  auto javadoc_blocks = javadoc_parser_.extractJavadocBlocks(content, lang_info.javadoc_style);
  
  // Associate existing docstrings with constructs (enhance what we found in AST)
  for (auto& construct : constructs) {
    if (!construct.docstring.has_value()) {
      // Look for javadoc comments near this construct
      for (const auto& javadoc : javadoc_blocks) {
        // Simple proximity check - could be made more sophisticated
        if (javadoc.location.line < construct.start_line && 
            construct.start_line - javadoc.location.line <= 10) {
          construct.docstring = javadoc.description.empty() ? javadoc.raw_content : javadoc.description;
          break;
        }
      }
    }
  }

  // Cleanup
  ts_tree_delete(tree);
  ts_parser_delete(parser);

  return constructs;
}
