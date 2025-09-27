/**
 * @file doc_cli.cpp
 * @brief Command-line interface implementation for Cesium documentation tools
 */
#include "doc_cli.h"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <set>
#include <cstring>
#include "docgen.h"
#include "dynlib.h"
#include "tree_sitter/api.h"
#ifdef _WIN32
#include <windows.h>
#endif

int CesiumDocCLI::run(int argc, char* argv[]) {
  if (argc < 1) {
    printUsage();
    return 1;
  }

  // Find the actual command (skip "doc" if present)
  int command_index = 0;
  if (argc > 0 && std::string(argv[0]) == "doc") {
    command_index = 1;
  }

  // If no command provided, show usage
  if (command_index >= argc) {
    printUsage();
    return 0;
  }

  std::string command = argv[command_index];

  // Handle help flags first
  if (command == "--help" || command == "-h") {
    printUsage();
    return 0;
  }

  if (command == "generate" || command == "gen") {
    return generateDocs(argc, argv);
  } else if (command == "list-parsers") {
    return listParsers(argc, argv);
  } else if (command == "init-config") {
    return initConfig(argc, argv);
  } else {
    std::cerr << "Unknown command: " << command << std::endl;
    printUsage();
    return 1;
  }
}

void CesiumDocCLI::printUsage() {
  std::cout << "Usage: cesium doc <command> [options]\n\n";
  std::cout << "Commands:\n";
  std::cout << "  generate, gen             Generate documentation\n";
  std::cout << "  list-parsers              List available language parsers\n";
  std::cout << "  init-config [filename]    Create default configuration file\n";
  std::cout << "\nOptions:\n";
  std::cout << "  --config <file>           Configuration file (for generate command)\n";
  std::cout << "  --help, -h               Show this help message\n";
}

int CesiumDocCLI::generateDocs(int argc, char* argv[]) {
  std::string config_path;
  bool config_specified = false;

  // Determine starting index for argument parsing
  int start_index = 1;  // Default case: {"generate", "--config", "file"}
  if (argc > 1 && std::string(argv[0]) == "doc") {
    // Case: {"doc", "generate", "--config", "file"}
    start_index = 2;
  } else if (argc > 2 && std::string(argv[1]) == "doc") {
    // Case: {"cesium", "doc", "generate", "--config", "file"}
    start_index = 3;
  }

  // Parse command line arguments
  for (int i = start_index; i < argc; i++) {
    if (std::string(argv[i]) == "--config" && i + 1 < argc) {
      config_path = argv[i + 1];
      config_specified = true;
      i++;
    }
  }

  // If no config was specified, check for default config file
  if (!config_specified) {
    if (std::filesystem::exists("cesium-doc-config.json")) {
      config_path = "cesium-doc-config.json";
      std::cout << "Using default configuration file: " << std::filesystem::absolute(config_path) << std::endl;
    } else {
      std::cerr << "No configuration file specified and cesium-doc-config.json not found." << std::endl;
      std::cerr << "Use --config <file> or create cesium-doc-config.json in current directory." << std::endl;
      return 1;
    }
  }

  CesiumDocExtractor extractor;
  if (!extractor.initialize(config_path)) {
    return 1;
  }

  extractor.extractDocs(config_path);
  std::cout << "Documentation generation complete!" << std::endl;
  return 0;
}

// Helper function to extract language from parser filename (fallback)
std::string extractLanguageFromFilename(const std::string& filename) {
  // Remove common prefixes and suffixes
  std::string name = filename;

  // Remove lib prefix if present
  if (name.starts_with("lib")) {
    name = name.substr(3);
  }

  // Remove tree-sitter- prefix
  if (name.starts_with("tree-sitter-")) {
    name = name.substr(12);
  } else if (name.starts_with("tree_sitter_")) {
    name = name.substr(13);
  }

  // Remove file extensions
  if (name.ends_with(".dll")) {
    name = name.substr(0, name.length() - 4);
  } else if (name.ends_with(".so")) {
    name = name.substr(0, name.length() - 3);
  }

  return name;
}

// Helper function to get language name from tree-sitter library with config context
std::pair<std::string, std::string> getLanguageFromParserWithConfig(const std::string& filepath,
                                                                    const std::string& config_lang_name = "") {
  std::string detected_name;
  std::string warning;

  try {
    // Load the library
    auto library = dynlib::loadLib(filepath);
    if (!library.isValid()) {
      if (!config_lang_name.empty()) {
        return {config_lang_name, ""};
      }
      return {extractLanguageFromFilename(std::filesystem::path(filepath).filename().string()), ""};
    }

    // Try basic filename-based discovery to find a working function
    std::string base_lang = extractLanguageFromFilename(std::filesystem::path(filepath).filename().string());
    if (!base_lang.empty()) {
      std::string func_name = "tree_sitter_" + base_lang;
      try {
        auto language_func = library.getFunction<const TSLanguage*(*)()>(func_name);
        if (language_func) {
          const TSLanguage* language = language_func();
          if (language) {
            // We successfully loaded a tree-sitter language - validate it with metadata
            const TSLanguageMetadata* metadata = ts_language_metadata(language);
            if (metadata) {
              // Metadata contains version info - we can verify this is a valid parser
              // Language name must be extracted from function name since API doesn't provide it
              detected_name = base_lang;
            } else {
              // No metadata available - might be an older parser
              detected_name = base_lang;
              if (warning.empty()) {
                warning = "Parser loaded but no metadata available (older parser?)";
              }
            }

            // Check for mismatch with config
            if (!config_lang_name.empty() && detected_name != config_lang_name) {
              if (!warning.empty()) warning += "; ";
              warning += "Config says '" + config_lang_name + "' but function suggests '" + detected_name + "'";
            }

            return {detected_name, warning};
          }
        }
      } catch (...) {
        // Discovery failed
      }
    }

  } catch (...) {
    // Library loading failed
  }

  // Fall back to config info if we have it
  if (!config_lang_name.empty()) {
    return {config_lang_name, "Unable to verify with parser"};
  }

  // Final fallback to filename parsing
  return {extractLanguageFromFilename(std::filesystem::path(filepath).filename().string()), ""};
}

// Simple wrapper for backward compatibility
std::string getLanguageFromParser(const std::string& filepath) {
  auto [lang_name, warning] = getLanguageFromParserWithConfig(filepath);
  return lang_name;
}

int CesiumDocCLI::listParsers(int argc, char* argv[]) {
  std::cout << "Available tree-sitter parsers (in priority order):\n\n";

  std::string config_path;
  bool config_specified = false;
  std::set<std::string> found_languages; // Track languages we've already found

  // Determine starting index for argument parsing (same logic as generateDocs)
  int start_index = 1;
  if (argc > 1 && std::string(argv[0]) == "doc") {
    start_index = 2;
  } else if (argc > 2 && std::string(argv[1]) == "doc") {
    start_index = 3;
  }

  // Parse command line arguments
  for (int i = start_index; i < argc; i++) {
    if (std::string(argv[i]) == "--config" && i + 1 < argc) {
      config_path = argv[i + 1];
      config_specified = true;
      i++;
    }
  }

  // If no config was specified, check for default config file
  bool has_config = false;
  if (!config_specified) {
    if (std::filesystem::exists("cesium-doc-config.json")) {
      config_path = "cesium-doc-config.json";
      has_config = true;
    }
  } else {
    has_config = std::filesystem::exists(config_path);
  }

  // 1. Show parsers from config file (highest priority)
  if (has_config) {
    std::cout << "1. From configuration file (" << config_path << "):\n";
    try {
      std::ifstream file(config_path);
      std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

      // Parse JSON to extract language -> library mappings
      size_t lang_pos = 0;
      bool found_any = false;
      while ((lang_pos = content.find("\"", lang_pos)) != std::string::npos) {
        size_t lang_end = content.find("\"", lang_pos + 1);
        if (lang_end == std::string::npos) break;

        std::string potential_lang = content.substr(lang_pos + 1, lang_end - lang_pos - 1);

        // Look for "library": after this language key
        size_t lib_search_start = lang_end;
        size_t lib_pos = content.find("\"library\":", lib_search_start);
        size_t next_lang = content.find("\"", lang_end + 1);

        // Check if this library belongs to current language
        if (lib_pos != std::string::npos && (next_lang == std::string::npos || lib_pos < next_lang)) {
          size_t lib_start = content.find("\"", lib_pos + 10);
          size_t lib_end = content.find("\"", lib_start + 1);
          if (lib_start != std::string::npos && lib_end != std::string::npos) {
            std::string library = content.substr(lib_start + 1, lib_end - lib_start - 1);
            std::string actual_file = dynlib::findLibraryFile(library, ".");

            // Get language info with config context
            auto [detected_lang, warning] = getLanguageFromParserWithConfig(
              (std::filesystem::path(".") / actual_file).string(), potential_lang);

            std::cout << "  " << detected_lang << ": " << actual_file;
            if (!warning.empty()) {
              std::cout << " \033[33m⚠ " << warning << "\033[0m";  // Yellow warning
            }
            std::cout << std::endl;

            found_languages.insert(detected_lang);
            found_any = true;
          }
        }

        lang_pos = lang_end + 1;
      }

      if (!found_any) {
        std::cout << "  (no parsers defined)" << std::endl;
      }
    } catch (const std::exception&) {
      std::cout << "  (error reading config file)" << std::endl;
    }
    std::cout << std::endl;
  }

  // 2. Show parsers from current directory
  std::cout << "2. From current directory:\n";
  try {
    bool found_any = false;
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
      std::string filename = entry.path().filename().string();
#ifdef _WIN32
      if (filename.find("tree-sitter-") != std::string::npos &&
          (filename.ends_with(".dll") || filename.ends_with(".so"))) {
#else
      if (filename.find("libtree-sitter-") == 0 && filename.ends_with(".so")) {
#endif
        std::filesystem::path full_path = std::filesystem::path(".") / filename;
        std::string language = getLanguageFromParser(full_path.string());
        if (found_languages.find(language) != found_languages.end()) {
          std::cout << "  " << language << ": " << filename << " (superseded by config)" << std::endl;
        } else {
          std::cout << "  " << language << ": " << filename << std::endl;
          found_languages.insert(language);
        }
        found_any = true;
      }
    }
    if (!found_any) {
      std::cout << "  (none found)" << std::endl;
    }
  } catch (const std::filesystem::filesystem_error&) {
    std::cout << "  (directory not accessible)" << std::endl;
  }
  std::cout << std::endl;

  // 3. Show system parsers (lowest priority)
  std::cout << "3. From system paths:\n";
  std::vector<std::string> system_paths;

#ifdef _WIN32
  // Windows system paths - could be expanded later
  system_paths = {
    // Currently no standard Windows paths defined
  };
#else
  // POSIX system paths
  system_paths = {
    "/usr/local/lib",
    "/usr/lib"
  };
#endif

  // Add program installation directory as fallback
  try {
    std::filesystem::path exe_path;
#ifdef _WIN32
    // On Windows, get the executable path
    char buffer[MAX_PATH];
    if (GetModuleFileNameA(NULL, buffer, MAX_PATH) > 0) {
      exe_path = std::filesystem::canonical(buffer);
    }
#else
    // On POSIX systems, try /proc/self/exe (Linux) or other methods
    if (std::filesystem::exists("/proc/self/exe")) {
      exe_path = std::filesystem::canonical("/proc/self/exe");
    }
#endif
    if (!exe_path.empty()) {
      std::filesystem::path install_dir = exe_path.parent_path();
      system_paths.push_back(install_dir.string());
    } else {
      // Fallback for debug builds or if we can't determine exe path
      system_paths.push_back("build/bin");
    }
  } catch (const std::exception&) {
    // Fallback for debug builds or if we can't determine exe path
    system_paths.push_back("build/bin");
  }

  if (system_paths.empty()) {
    std::cout << "  (no system paths available)" << std::endl;
  }

  for (const auto& path : system_paths) {
    std::cout << "  In " << path << ":\n";
    try {
      bool found_any = false;
      for (const auto& entry : std::filesystem::directory_iterator(path)) {
        std::string filename = entry.path().filename().string();
#ifdef _WIN32
        if (filename.find("tree-sitter-") != std::string::npos &&
            (filename.ends_with(".dll") || filename.ends_with(".so"))) {
#else
        if (filename.find("libtree-sitter-") == 0 && filename.ends_with(".so")) {
#endif
          std::filesystem::path full_path = std::filesystem::path(path) / filename;
          std::string language = getLanguageFromParser(full_path.string());
          if (found_languages.find(language) != found_languages.end()) {
            std::cout << "    " << language << ": " << filename << " (superseded)" << std::endl;
          } else {
            std::cout << "    " << language << ": " << filename << std::endl;
            found_languages.insert(language);
          }
          found_any = true;
        }
      }
      if (!found_any) {
        std::cout << "    (none found)" << std::endl;
      }
    } catch (const std::filesystem::filesystem_error&) {
      std::cout << "    (directory not accessible)" << std::endl;
    }
  }

  return 0;
}

int CesiumDocCLI::initConfig(int argc, char* argv[]) {
  std::string config_path = "doc-config.json";

  // Determine config file path index
  int config_index = 1;  // Default case: {"init-config", "filename"}
  if (argc > 1 && std::string(argv[0]) == "doc") {
    // Case: {"doc", "init-config", "filename"}
    config_index = 2;
  } else if (argc > 2 && std::string(argv[1]) == "doc") {
    // Case: {"cesium", "doc", "init-config", "filename"}
    config_index = 3;
  }

  if (argc > config_index) {
    config_path = argv[config_index];
  }

  // Write the config as a simple JSON string to avoid JsonDoc issues
  std::ofstream file(config_path);
  if (!file.is_open()) {
    std::cerr << "Failed to open file for writing: " << config_path << std::endl;
    return 1;
  }

  file << R"({
  "languages": {
    "cpp": {
      "library": "tree-sitter-cpp.so",
      "function": "tree_sitter_cpp",
      "extensions": [".cpp", ".hpp", ".cc", ".h", ".cxx"],
      "javadoc_style": "/** */"
    }
  },
  "source_directories": ["cesium/src/", "cesium/include/"],
  "output_directory": "docs/extracted/",
  "exclude_patterns": ["**/test/**", "**/*_test.*"]
})";

  file.close();
  std::cout << "Created default configuration: " << config_path << std::endl;
  return 0;
}
