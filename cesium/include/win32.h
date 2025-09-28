#pragma once

/*
the following comment forces all files that include this to assume that they
are using things defined in this file explicitly, even if they don't.
This is because the forward declarations of windows.h are not getting picked up
by the include-what-you-use (IWYU) tool, part of clangd, leading to a lot of
false positives.
*/
// IWYU pragma: always_keep

#ifdef _WIN32
  // I don't really want to define this here, but I don't want to figure out
  // how to include Windows headers without pulling in all the junk yet and
  // doing so in a private way that doesn't affect other files.
  namespace win32 {
    // #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX  // https://learn.microsoft.com/en-us/archive/msdn-magazine/2016/september/c-unicode-encoding-conversions-with-stl-strings-and-win32-apis#converting-from-utf-8-to-utf-16-multibytetowidechar-in-action
    #include <windows.h> // IWYU pragma: export

    using win32::GetProcAddress;
    using win32::FreeLibrary;
    using win32::LoadLibrary;
    using win32::HMODULE;
    using win32::SetErrorMode;
    using win32::GetConsoleOutputCP;
    using win32::SetConsoleOutputCP;
    using win32::GetLastError;
    using win32::DWORD;
    using win32::LPTSTR;
    using win32::FormatMessage;
    using win32::WideCharToMultiByte;
    using win32::LocalFree;
  }
#endif

// adapted from: https://github.com/MicrosoftDocs/cpp-docs/issues/1915#issuecomment-589644386
class ConsoleUTF8 {
  public:
    ConsoleUTF8();
    ~ConsoleUTF8();

  private:
    unsigned int m_original_cp;
};
