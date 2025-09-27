/**
 * @file main.cpp
 * @brief Main entry point for Cesium CLI tool
 */
#include <iostream>
#include "version.h"
#include "doc_cli.h"

#ifdef _DEBUG
#include "debug_utils.h"
#endif

/**
 * @brief Print main help message showing available commands
 */
void printHelp() {
  std::cout << "Usage: cesium [command] [options]\n\n";
  std::cout << "Commands:\n";
  std::cout << "  doc                       Documentation tools\n";
  std::cout << "  --help, -h               Show this help message\n";
  std::cout << "  --version, -v            Show version information\n";
  std::cout << "\nFor detailed help on a specific command:\n";
  std::cout << "  cesium doc               Show documentation command help\n";
}

/**
 * @brief Main entry point for Cesium CLI application
 * 
 * Processes command-line arguments and dispatches to appropriate handlers.
 * Supports documentation tools, help, and version commands.
 * 
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return Exit code (0 for success, 1 for error)
 */
int main(int argc, char *argv[])
{
#ifdef _DEBUG
  debug::suppressErrorDialogs();
#endif
  
  if (argc > 1) {
    std::string arg = argv[1];
    
    // Documentation tools subcommand
    if (arg == "doc") {
      CesiumDocCLI cli;
      return cli.run(argc - 1, argv + 1); // Skip "cesium" from args
    }
    
    // Help command
    if (arg == "--help" || arg == "-h") {
      printHelp();
      return 0;
    }
    
    // Version information
    if (arg == "--version" || arg == "-v") {
      std::cout << cesium::version::getFullVersionInfo() << std::endl;
      return 0;
    }
    
    // Unknown command
    std::cerr << "Unknown command: " << arg << std::endl;
    std::cerr << "Use 'cesium --help' for usage information." << std::endl;
    return 1;
  }

  // No arguments provided - show friendly greeting
  std::cout << "Hello from Cesium!" << std::endl;
  return 0;
}
