#include "win32.h"

// #ifdef _WIN32
//   namespace win32internal {
//     // #define WIN32_LEAN_AND_MEAN
//     #define NOMINMAX  // https://learn.microsoft.com/en-us/archive/msdn-magazine/2016/september/c-unicode-encoding-conversions-with-stl-strings-and-win32-apis#converting-from-utf-8-to-utf-16-multibytetowidechar-in-action
//     #include <Windows.h>
//     // Clean up problematic Windows macros that conflict with our identifiers
//     // This must be done after Windows headers are included (directly or indirectly)
//     // #ifdef ERROR
//     //   #undef ERROR
//     // #endif
//     // #ifdef SUCCESS
//     //   #undef SUCCESS
//     // #endif
//     // #ifdef INFO
//     //   #undef INFO
//     // #endif
//   }
// #endif

// adapted from: https://github.com/MicrosoftDocs/cpp-docs/issues/1915#issuecomment-589644386
ConsoleUTF8::ConsoleUTF8() {
  #ifdef _WIN32
    m_original_cp = win32::GetConsoleOutputCP();
    win32::SetConsoleOutputCP(CP_UTF8);
  #endif
}

ConsoleUTF8::~ConsoleUTF8() {
  #ifdef _WIN32
    win32::SetConsoleOutputCP(m_original_cp);
  #endif
}
