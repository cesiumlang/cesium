/**
@brief Main entry point for Cesium CLI tool
*/
#include <iostream>
#include <stdexcept>
#include <exception>
#include "version.h"
#include <backend/doc/doc_cli.h>
#include <backend/core/win32.h>
#include <backend/core/cli_utils.h>

#ifdef _DEBUG
#include <backend/core/debug.h>
#endif

/**
@brief Print main help message showing available commands
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
@brief Main entry point for Cesium CLI application

Processes command-line arguments and dispatches to appropriate handlers.
Supports documentation tools, help, and version commands.

@param argc Number of command-line arguments
@param argv Array of command-line argument strings
@return Exit code (0 for success, 1 for error)
*/
int main(int argc, char *argv[])
{
  try {
    #ifdef _DEBUG
      debug::suppressErrorDialogs();
    #endif
    ConsoleUTF8 set_console_to_utf8;

    CLILogger::debug("main: Starting Cesium CLI with " + std::to_string(argc) + " arguments");
    for (int i = 0; i < argc; i++) {
      CLILogger::debuglow("main: argv[" + std::to_string(i) + "] = '" + std::string(argv[i]) + "'");
    }

    if (argc > 1) {
      std::string arg = argv[1];
      CLILogger::debug("main: Processing command: " + arg);

      // Documentation tools subcommand
      if (arg == "doc") {
        CLILogger::debug("main: Invoking documentation CLI");
        try {
          CesiumDocCLI cli;
          int result = cli.run(argc - 1, argv + 1); // Skip "cesium" from args
          CLILogger::debug("main: Documentation CLI completed with exit code: " + std::to_string(result));
          return result;
        } catch (const std::exception& e) {
          CLILogger::error("main: Documentation CLI failed with exception: " + std::string(e.what()));
          std::cerr << "Error running documentation command: " << e.what() << std::endl;
          return 1;
        } catch (...) {
          CLILogger::error("main: Documentation CLI failed with unknown exception");
          std::cerr << "Unknown error occurred while running documentation command" << std::endl;
          return 1;
        }
      }

      // Help command
      if (arg == "--help" || arg == "-h") {
        CLILogger::debug("main: Showing help information");
        try {
          printHelp();
          return 0;
        } catch (const std::exception& e) {
          CLILogger::error("main: Failed to display help: " + std::string(e.what()));
          std::cerr << "Error displaying help: " << e.what() << std::endl;
          return 1;
        }
      }

      // Version information
      if (arg == "--version" || arg == "-v") {
        CLILogger::debug("main: Showing version information");
        try {
          std::string version_info = cesium::version::getFullVersionInfo();
          std::cout << version_info << std::endl;
          CLILogger::debug("main: Successfully displayed version information");
          return 0;
        } catch (const std::exception& e) {
          CLILogger::error("main: Failed to get version information: " + std::string(e.what()));
          std::cerr << "Error retrieving version information: " << e.what() << std::endl;
          return 1;
        }
      }

      // Unknown command
      CLILogger::warning("main: Unknown command provided: " + arg);
      std::cerr << "Unknown command: " << arg << std::endl;
      std::cerr << "Use 'cesium --help' for usage information." << std::endl;
      return 1;
    }

    // No arguments provided - show friendly greeting
    CLILogger::debug("main: No arguments provided, showing greeting");
    std::cout << "Hello from Cesium!" << std::endl;
    CLILogger::debug("main: Cesium CLI completed successfully");
    return 0;

  } catch (const std::exception& e) {
    // Catch any unhandled exceptions at the top level
    CLILogger::error("main: Unhandled exception in main: " + std::string(e.what()));
    std::cerr << "Fatal error: " << e.what() << std::endl;
    std::cerr << "Please report this issue if it persists." << std::endl;
    return 1;
  } catch (...) {
    // Catch any other exceptions
    CLILogger::error("main: Unhandled unknown exception in main");
    std::cerr << "Fatal unknown error occurred" << std::endl;
    std::cerr << "Please report this issue if it persists." << std::endl;
    return 1;
  }
}
