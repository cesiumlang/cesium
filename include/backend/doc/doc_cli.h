/**
@brief Command-line interface for Cesium documentation generation tools
*/
#pragma once

// #include "cli_utils.h"

/**
@brief Main command-line interface handler for Cesium doc commands

Processes command-line arguments and dispatches to appropriate subcommands
for documentation generation, parser listing, and configuration management.
*/
class CesiumDocCLI {
  public:
    /**
    @brief Main entry point for doc CLI
    @param argc Command line argument count
    @param argv Command line argument vector
    @return Exit code (0 for success, non-zero for error)
    */
    int run(int argc, char* argv[]);

  private:
    /**
    @brief Print help message showing available commands
    */
    void printUsage();

    /**
    @brief Print help message for extract command
    */
    void printExtractUsage();

    /**
    @brief Print help message for generate command
    */
    void printGenerateUsage();

    /**
    @brief Extract docstrings and create markdown snippets
    @return Exit code
    */
    int extractDocs(int argc, char* argv[]);

    /**
    @brief Generate structured documentation from extracted snippets
    @return Exit code
    */
    int generateDocs(int argc, char* argv[]);

    /**
    @brief List available Tree-sitter parsers
    @return Exit code
    */
    int listParsers(int argc, char* argv[]);

    /**
    @brief Initialize configuration file
    @return Exit code
    */
    int initConfig(int argc, char* argv[]);

    /**
    @brief Prune orphaned documentation files
    
    Removes documentation files that are no longer tracked by cache or whose
    source files have been deleted. Supports dry-run mode for preview.
    
    @param argc Command line argument count
    @param argv Command line argument vector
    @return Exit code (0 for success, non-zero for error)
    */
    int pruneDocs(int argc, char* argv[]);

    /**
    @brief Print help message for prune command
    
    Displays usage information, options, and examples for the prune command.
    */
    void printPruneUsage();
};
