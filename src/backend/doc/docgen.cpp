/**
@brief Core documentation extraction and generation implementation
*/
#include <backend/doc/docgen.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <backend/core/json.h>
#include <backend/core/cli_utils.h>

bool CesiumDocExtractor::initialize(const std::string& config_path) {
  auto config = JsonDoc::fromFile(config_path);
  if (!config) {
    std::cerr << "Failed to load configuration from: " << config_path << std::endl;
    return false;
  }

  // Initialize cache
  const JsonDoc& config_ref = *config;
  
  // Configure logging if specified in config
  JsonValue logging_config = config_ref["logging"];
  if (!logging_config.isNull()) {
    CLILogger::configureFromFile(config_path);
  }
  
  std::string extract_dir = static_cast<std::string>(config_ref["extract_directory"]);
  
  // Normalize path to avoid double slashes
  std::filesystem::path cache_path = std::filesystem::path(extract_dir) / ".cesium-cache.json";
  std::string cache_file = cache_path.string();
  
  cache_ = std::make_unique<DocumentationCache>(cache_file);
  cache_->load();

  // Load language parsers
  JsonValue languages = config_ref["languages"];
  
  languages.forEachObject([&](const std::string& lang_name, JsonValue lang_config) {
    if (loader_.loadLanguage(lang_name, lang_config, config_path)) {
      std::cout << "Loaded " << lang_name << " parser" << std::endl;
    } else {
      std::cerr << "Warning: Failed to load " << lang_name << " parser" << std::endl;
    }
  });

  return true;
}

bool CesiumDocExtractor::extract(const std::string& config_path,
                                  const std::string& source_override,
                                  const std::string& extract_dir_override) {
  auto config = JsonDoc::fromFile(config_path);
  if (!config) return false;

  const JsonDoc& config_ref = *config;
  std::vector<CodeConstruct> all_constructs;

  // Determine extract directory
  std::string extract_dir = extract_dir_override.empty() ? 
    static_cast<std::string>(config_ref["extract_directory"]) : extract_dir_override;
  
  // Create extract directory if it doesn't exist
  CLILogger::debug("CesiumDocExtractor::extract: Creating extract directory: " + extract_dir);
  try {
    bool created = std::filesystem::create_directories(extract_dir);
    if (created) {
      CLILogger::debug("CesiumDocExtractor::extract: Successfully created extract directory: " + extract_dir);
    } else {
      CLILogger::debug("CesiumDocExtractor::extract: Extract directory already exists: " + extract_dir);
    }
  } catch (const std::filesystem::filesystem_error& e) {
    CLILogger::error("CesiumDocExtractor::extract: Failed to create extract directory '" + extract_dir + "': " + e.what());
    return false;
  }

  // Verify cache integrity at start and prune orphaned files
  if (cache_ && !cache_->verifyIntegrity(extract_dir)) {
    std::cout << "Cache integrity issues detected - pruning orphaned files" << std::endl;
    size_t pruned = cache_->pruneOrphanedFiles(extract_dir, false);
    if (pruned > 0) {
      std::cout << "Removed " << pruned << " orphaned files" << std::endl;
    }
  }

  // Determine source directories/files to process
  if (!source_override.empty()) {
    // Process specific source override
    std::cout << "Processing source override: " << source_override << std::endl;
    
    // Check if the source override path exists
    CLILogger::debug("CesiumDocExtractor::extract: Checking if source override exists: " + source_override);
    if (!std::filesystem::exists(source_override)) {
      CLILogger::error("CesiumDocExtractor::extract: Source override path does not exist: " + source_override);
      CLILogger::stderr_msg("Please check the path and try again.");
      return false;
    }
    CLILogger::debug("CesiumDocExtractor::extract: Source override path exists: " + source_override);
    
    if (std::filesystem::is_directory(source_override)) {
      CLILogger::debug("CesiumDocExtractor::extract: Source override is directory, starting recursive iteration: " + source_override);
      try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(source_override)) {
        if (entry.is_regular_file()) {
            std::string filepath = entry.path().string();
            CLILogger::debuglow("CesiumDocExtractor::extract: Found file in source override: " + filepath);
            if (needsExtraction(filepath, extract_dir)) {
              auto [lang_name, lang_info] = loader_.getLanguageForFile(filepath);
              if (lang_info) {
                std::cout << "Extracting " << filepath << " as " << lang_name << std::endl;
                CLILogger::debug("CesiumDocExtractor::extract: Extracting constructs from file: " + filepath);
                auto constructs = extractAllConstructs(filepath, *lang_info);
                all_constructs.insert(all_constructs.end(), constructs.begin(), constructs.end());
                CLILogger::debuglow("CesiumDocExtractor::extract: Added " + std::to_string(constructs.size()) + " constructs from " + filepath);
              } else {
                CLILogger::debuglow("CesiumDocExtractor::extract: No language parser found for file: " + filepath);
              }
            } else {
              CLILogger::debuglow("CesiumDocExtractor::extract: File does not need extraction (up to date): " + filepath);
            }
          }
        }
      } catch (const std::filesystem::filesystem_error& e) {
        CLILogger::error("CesiumDocExtractor::extract: Error iterating source override directory '" + source_override + "': " + e.what());
        return false;
      }
      CLILogger::debug("CesiumDocExtractor::extract: Completed recursive iteration of source override directory");
    } else if (std::filesystem::is_regular_file(source_override)) {
      CLILogger::debug("CesiumDocExtractor::extract: Source override is regular file: " + source_override);
      if (cache_->needsExtraction(source_override)) {
        auto [lang_name, lang_info] = loader_.getLanguageForFile(source_override);
        if (lang_info) {
          std::cout << "Extracting " << source_override << " as " << lang_name << std::endl;
          auto constructs = extractAllConstructs(source_override, *lang_info);
          all_constructs.insert(all_constructs.end(), constructs.begin(), constructs.end());
          
          // Update cache with extracted files (placeholder - will be enhanced)
          std::vector<std::string> generated_files; // TODO: Get actual generated files
          cache_->updateFile(source_override, generated_files, constructs.size(), lang_name);
          
          // Save cache immediately for crash resilience
          cache_->saveImmediately();
        }
      }
    } else {
      CLILogger::error("Source override path is neither a file nor directory: " + source_override);
      CLILogger::stderr_msg("Please specify a valid file or directory path.");
      return false;
    }
  } else {
    // Process configured source directories
    JsonValue source_dirs_value = config_ref["source_directories"];
    
    source_dirs_value.forEachArray([&](size_t idx, JsonValue dir_value) {
      std::string dir_str = static_cast<std::string>(dir_value);
      std::cout << "Processing directory: " << dir_str << std::endl;

      // Check if directory exists before trying to iterate
      if (!std::filesystem::exists(dir_str)) {
        CLILogger::error("Source directory does not exist: " + dir_str);
        CLILogger::stderr_msg("Please check your configuration file and update source_directories to point to valid paths.");
        CLILogger::debug("Skipping non-existent directory: " + dir_str);
        return; // Skip this directory and continue with others
      }
      
      if (!std::filesystem::is_directory(dir_str)) {
        CLILogger::error("Source path is not a directory: " + dir_str);
        CLILogger::stderr_msg("Please check your configuration file - source_directories should contain directory paths only.");
        CLILogger::debug("Skipping non-directory path: " + dir_str);
        return; // Skip this entry and continue with others
      }

      CLILogger::debug("CesiumDocExtractor::extract: Starting recursive iteration of configured directory: " + dir_str);
      try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_str)) {
        if (entry.is_regular_file()) {
            std::string filepath = entry.path().string();
            CLILogger::debuglow("CesiumDocExtractor::extract: Found file in configured directory: " + filepath);
            if (cache_->needsExtraction(filepath)) {
              auto [lang_name, lang_info] = loader_.getLanguageForFile(filepath);
              if (lang_info) {
                std::cout << "Extracting " << filepath << " as " << lang_name << std::endl;
                CLILogger::debug("CesiumDocExtractor::extract: Extracting constructs from file: " + filepath);
                auto constructs = extractAllConstructs(filepath, *lang_info);
                all_constructs.insert(all_constructs.end(), constructs.begin(), constructs.end());
                CLILogger::debuglow("CesiumDocExtractor::extract: Added " + std::to_string(constructs.size()) + " constructs from " + filepath);
              } else {
                CLILogger::debuglow("CesiumDocExtractor::extract: No language parser found for file: " + filepath);
              }
            } else {
              CLILogger::debuglow("CesiumDocExtractor::extract: File does not need extraction (cached): " + filepath);
            }
          }
        }
      } catch (const std::filesystem::filesystem_error& e) {
        CLILogger::error("CesiumDocExtractor::extract: Error iterating configured directory '" + dir_str + "': " + e.what());
        CLILogger::stderr_msg("Failed to process directory. Please check permissions and path validity.");
        return; // Skip this directory and continue with others
      }
      CLILogger::debug("CesiumDocExtractor::extract: Completed recursive iteration of directory: " + dir_str);
    });
  }

  // Generate markdown snippets to extract directory
  std::cout << "Creating " << all_constructs.size() << " markdown snippets in " << extract_dir << std::endl;
  auto generated_files = markdown_generator_.generateMarkdownFromConstructs(all_constructs, extract_dir);
  
  // Now update cache with the actual generated files
  // Group generated files by source file (for now, assume all come from the last processed file)
  // TODO: This is a simplification - we need better tracking of which constructs came from which files
  if (cache_ && !all_constructs.empty()) {
    // For now, associate all generated files with the source file of the first construct
    std::string source_file = all_constructs[0].filename;
    auto [lang_name, lang_info] = loader_.getLanguageForFile(source_file);
    if (lang_info) {
      cache_->updateFile(source_file, generated_files, all_constructs.size(), lang_name);
      
      // Save cache immediately for crash resilience
      cache_->saveImmediately();
    }
  }
  
  // Save cache after successful extraction
  if (cache_) {
    cache_->save();
    auto [file_count, generated_count] = cache_->getStats();
    std::cout << "Cache updated: " << file_count << " files tracked, " 
              << generated_count << " outputs generated" << std::endl;
  }
  
  return true;
}

bool CesiumDocExtractor::generate(const std::string& config_path) {
  auto config = JsonDoc::fromFile(config_path);
  if (!config) return false;

  const JsonDoc& config_ref = *config;
  std::string extract_dir = static_cast<std::string>(config_ref["extract_directory"]);
  std::string output_dir = static_cast<std::string>(config_ref["output_directory"]);

  // First, run extract to ensure all changed files are processed
  if (!extract(config_path)) {
    std::cerr << "Failed to extract documentation snippets" << std::endl;
    return false;
  }

  // Then process the markdown snippets into structured documentation
  std::cout << "Generating structured documentation from snippets in " << extract_dir << std::endl;
  processMarkdownSnippets(extract_dir, output_dir);
  
  return true;
}

void CesiumDocExtractor::extractDocs(const std::string& config_path) {
  // Legacy method - just call generate which does extract + generate
  generate(config_path);
}

std::vector<DocstringBlock> CesiumDocExtractor::extractFromFile(const std::string& filepath,
                                                                const LanguageInfo& lang_info) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filepath << std::endl;
    return {};
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
  file.close();

  // Extract docstring comments
  auto docstring_blocks = docstring_parser_.extractDocstrings(content, lang_info.docstring_style);

  // Parse with tree-sitter
  TSParser* parser = ts_parser_new();
  ts_parser_set_language(parser, lang_info.language);
  TSTree* tree = ts_parser_parse_string(parser, nullptr, content.c_str(), content.length());

  // Associate comments with AST nodes
  doc_associator_.associateDocsWithNodes(docstring_blocks, tree, content);

  // Cleanup
  ts_tree_delete(tree);
  ts_parser_delete(parser);

  return docstring_blocks;
}

std::vector<CodeConstruct> CesiumDocExtractor::extractAllConstructs(const std::string& filepath,
                                                                   const LanguageInfo& lang_info) {
  CLILogger::debug("extractAllConstructs: Starting extraction for file: " + filepath);
  
  std::ifstream file(filepath);
  if (!file.is_open()) {
    CLILogger::error("extractAllConstructs: Failed to open file: " + filepath);
    std::cerr << "Failed to open file: " << filepath << std::endl;
    return {};
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
  file.close();
  
  CLILogger::debug("extractAllConstructs: Successfully read file content, size: " + std::to_string(content.length()) + " bytes");

  // Parse with tree-sitter
  CLILogger::debug("extractAllConstructs: Creating tree-sitter parser for language: " + lang_info.function_name);
  TSParser* parser = ts_parser_new();
  if (!parser) {
    CLILogger::error("extractAllConstructs: Failed to create tree-sitter parser");
    return {};
  }
  
  CLILogger::debug("extractAllConstructs: Setting tree-sitter language");
  if (!ts_parser_set_language(parser, lang_info.language)) {
    CLILogger::error("extractAllConstructs: Failed to set tree-sitter language");
    ts_parser_delete(parser);
    return {};
  }
  
  CLILogger::debug("extractAllConstructs: Parsing content with tree-sitter (" + std::to_string(content.length()) + " bytes)");
  TSTree* tree = ts_parser_parse_string(parser, nullptr, content.c_str(), content.length());
  if (!tree) {
    CLILogger::error("extractAllConstructs: Tree-sitter parsing failed, returned null tree");
    ts_parser_delete(parser);
    return {};
  }
  
  TSNode root = ts_tree_root_node(tree);
  CLILogger::debug("extractAllConstructs: Tree-sitter parsing successful, root node type: " + std::string(ts_node_type(root)));

  // Extract all code constructs from AST
  CLILogger::debug("extractAllConstructs: Starting AST construct extraction");
  std::vector<CodeConstruct> constructs = ast_extractor_.extractConstructs(tree, content, filepath);
  CLILogger::debug("extractAllConstructs: AST extraction completed, found " + std::to_string(constructs.size()) + " constructs");

  // Optional: Extract docstring comments and associate them with constructs
  CLILogger::debug("extractAllConstructs: Extracting docstring comments with style: '" + lang_info.docstring_style + "'");
  auto docstring_blocks = docstring_parser_.extractDocstrings(content, lang_info.docstring_style);
  CLILogger::debug("extractAllConstructs: Found " + std::to_string(docstring_blocks.size()) + " docstring blocks");
  
  // Associate existing docstrings with constructs (enhance what we found in AST)
  CLILogger::debug("extractAllConstructs: Associating docstrings with constructs");
  int associations_made = 0;
  for (auto& construct : constructs) {
    if (!construct.docstring.has_value()) {
      // Look for docstring comments near this construct
      for (const auto& docstring : docstring_blocks) {
        // Simple proximity check - could be made more sophisticated
        if (docstring.location.line < construct.start_line && 
            construct.start_line - docstring.location.line <= 10) {
          construct.docstring = docstring.description.empty() ? docstring.raw_content : docstring.description;
          associations_made++;
          CLILogger::debug("extractAllConstructs: Associated docstring with construct '" + construct.name + "' (line " + std::to_string(construct.start_line) + ")");
          break;
        }
      }
    }
  }
  CLILogger::debug("extractAllConstructs: Made " + std::to_string(associations_made) + " docstring associations");

  // Cleanup
  CLILogger::debug("extractAllConstructs: Cleaning up tree-sitter resources");
  ts_tree_delete(tree);
  ts_parser_delete(parser);
  
  CLILogger::debug("extractAllConstructs: Completed extraction for " + filepath + ", returning " + std::to_string(constructs.size()) + " constructs");
  return constructs;
}

bool CesiumDocExtractor::needsExtraction(const std::string& source_path, const std::string& extract_dir) {
  CLILogger::debuglow2("CesiumDocExtractor::needsExtraction: Checking if extraction needed for: " + source_path);
  
  // Convert source file path to expected markdown snippet path
  std::filesystem::path source_file(source_path);
  std::string base_name = source_file.stem().string();
  std::string snippet_path = extract_dir + "/" + base_name + ".md";
  
  CLILogger::debuglow2("CesiumDocExtractor::needsExtraction: Expected snippet path: " + snippet_path);
  
  // If snippet doesn't exist, need extraction
  try {
    if (!std::filesystem::exists(snippet_path)) {
      CLILogger::debuglow2("CesiumDocExtractor::needsExtraction: Snippet does not exist, extraction needed");
      return true;
    }
  } catch (const std::filesystem::filesystem_error& e) {
    CLILogger::error("CesiumDocExtractor::needsExtraction: Error checking snippet existence '" + snippet_path + "': " + e.what());
    return true; // Default to extraction if we can't check
  }
  
  // Check if source file is newer than snippet
  try {
    auto source_time = std::filesystem::last_write_time(source_path);
    auto snippet_time = std::filesystem::last_write_time(snippet_path);
    
    bool needs_extraction = source_time > snippet_time;
    CLILogger::debuglow2("CesiumDocExtractor::needsExtraction: Source newer than snippet: " + std::string(needs_extraction ? "true" : "false"));
    return needs_extraction;
  } catch (const std::filesystem::filesystem_error& e) {
    CLILogger::error("CesiumDocExtractor::needsExtraction: Error comparing file times for '" + source_path + "' and '" + snippet_path + "': " + e.what());
    return true; // Default to extraction if we can't check times
  }
}

void CesiumDocExtractor::processMarkdownSnippets(const std::string& extract_dir, const std::string& output_dir) {
  CLILogger::debug("CesiumDocExtractor::processMarkdownSnippets: Processing snippets from '" + extract_dir + "' to '" + output_dir + "'");
  
  // Placeholder function for processing markdown snippets into structured documentation
  // This will be implemented later to handle organization, cross-references, etc.
  
  std::cout << "Processing markdown snippets from " << extract_dir << " to " << output_dir << std::endl;
  
  // Create output directory
  CLILogger::debug("CesiumDocExtractor::processMarkdownSnippets: Creating output directory: " + output_dir);
  try {
    bool created = std::filesystem::create_directories(output_dir);
    if (created) {
      CLILogger::debug("CesiumDocExtractor::processMarkdownSnippets: Successfully created output directory: " + output_dir);
    } else {
      CLILogger::debug("CesiumDocExtractor::processMarkdownSnippets: Output directory already exists: " + output_dir);
    }
  } catch (const std::filesystem::filesystem_error& e) {
    CLILogger::error("CesiumDocExtractor::processMarkdownSnippets: Failed to create output directory '" + output_dir + "': " + e.what());
    return;
  }
  
  // For now, just copy snippets to output directory
  // TODO: Implement proper snippet processing, organization, cross-referencing
  CLILogger::debug("CesiumDocExtractor::processMarkdownSnippets: Iterating snippets in extract directory");
  try {
    int processed_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(extract_dir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".md") {
        std::string source_path = entry.path().string();
        std::filesystem::path dest = std::filesystem::path(output_dir) / entry.path().filename();
        std::string dest_path = dest.string();
        
        CLILogger::debuglow("CesiumDocExtractor::processMarkdownSnippets: Copying snippet '" + source_path + "' to '" + dest_path + "'");
        try {
          std::filesystem::copy_file(entry.path(), dest, std::filesystem::copy_options::overwrite_existing);
          std::cout << "Processed snippet: " << entry.path().filename() << std::endl;
          processed_count++;
        } catch (const std::filesystem::filesystem_error& e) {
          CLILogger::error("CesiumDocExtractor::processMarkdownSnippets: Failed to copy snippet '" + source_path + "' to '" + dest_path + "': " + e.what());
        }
      }
    }
    CLILogger::debug("CesiumDocExtractor::processMarkdownSnippets: Successfully processed " + std::to_string(processed_count) + " markdown snippets");
  } catch (const std::filesystem::filesystem_error& e) {
    CLILogger::error("CesiumDocExtractor::processMarkdownSnippets: Error iterating extract directory '" + extract_dir + "': " + e.what());
    return;
  }
  
  std::cout << "Snippet processing complete" << std::endl;
  CLILogger::debug("CesiumDocExtractor::processMarkdownSnippets: Completed snippet processing");
}
