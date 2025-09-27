/**
 * @file debug_utils.cpp
 * @brief Debugging utilities implementation
 */
#include "debug_utils.h"
#ifdef _WIN32
#include <crtdbg.h>
#include <windows.h>
#endif

namespace debug {
  void suppressErrorDialogs() {
#ifdef _WIN32
    // Disable Windows error reporting dialogs
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
  }
}