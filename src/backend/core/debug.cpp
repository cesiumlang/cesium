/**
@brief Debugging utilities implementation
*/
#include <backend/core/debug.h>
#ifdef _WIN32
  #include <crtdbg.h>
  #include <backend/core/win32.h>
#endif

namespace debug {
  void suppressErrorDialogs() {
    #ifdef _WIN32
      // Disable Windows error reporting dialogs
      SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX); //(0x0001 | 0x0002 | 0x8000);
      _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
      _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
      _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
      _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    #endif
  }
}
