/**
 * @file doc_cli.h
 * @brief Command-line interface for Cesium documentation generation tools
 */
#pragma once

/**
 * @class CesiumDocCLI
 * @brief Main command-line interface handler for Cesium doc commands
 * 
 * Processes command-line arguments and dispatches to appropriate subcommands
 * for documentation generation, parser listing, and configuration management.
 */
class CesiumDocCLI {
  public:
    /**
     * @brief Main entry point for doc CLI
     * @param argc Command line argument count
     * @param argv Command line argument vector
     * @return Exit code (0 for success, non-zero for error)
     */
    int run(int argc, char* argv[]);

  private:
    /**
     * @brief Print help message showing available commands
     */
    void printUsage();
    
    /**
     * @brief Generate documentation from source files
     * @return Exit code
     */
    int generateDocs(int argc, char* argv[]);
    
    /**
     * @brief List available Tree-sitter parsers
     * @return Exit code
     */
    int listParsers(int argc, char* argv[]);
    
    /**
     * @brief Initialize configuration file
     * @return Exit code
     */
    int initConfig(int argc, char* argv[]);
};