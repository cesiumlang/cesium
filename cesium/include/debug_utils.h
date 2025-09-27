/**
 * @file debug_utils.h
 * @brief Debugging utilities for development builds
 */
#pragma once

namespace debug {
  /**
   * @brief Suppress Windows error dialog boxes in debug builds
   * 
   * Prevents popup dialogs from appearing when debugging, allowing
   * automated testing and cleaner console output.
   */
  void suppressErrorDialogs();
}