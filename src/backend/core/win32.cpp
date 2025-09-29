/**
@brief Implementation of Windows-specific platform utilities
*/
#include <backend/core/win32.h>

// adapted from: https://github.com/MicrosoftDocs/cpp-docs/issues/1915#issuecomment-589644386
ConsoleUTF8::ConsoleUTF8() {
  #ifdef _WIN32
    m_original_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
  #endif
}

ConsoleUTF8::~ConsoleUTF8() {
  #ifdef _WIN32
    SetConsoleOutputCP(m_original_cp);
  #endif
}
