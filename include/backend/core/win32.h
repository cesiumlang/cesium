/**
@brief Windows-specific platform utilities and UTF-8 console support
*/
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

  // #define UTF8_KEEP_WIN32_API
  #include <utf8/utf8.h> // IWYU pragma: export
  #include <windows.h> // IWYU pragma: export

  // namespace win32 {
  //   #include <windows.h> // IWYU pragma: export
  // }
#endif

/**
@brief RAII wrapper for Windows console UTF-8 code page management

Automatically sets the Windows console code page to UTF-8 (CP_UTF8) on
construction and restores the original code page on destruction. This ensures
proper display of Unicode characters in console output.

Adapted from: https://github.com/MicrosoftDocs/cpp-docs/issues/1915#issuecomment-589644386
*/
class ConsoleUTF8 {
  public:
    /**
    @brief Constructor - saves current console code page and sets to UTF-8
    */
    ConsoleUTF8();
    
    /**
    @brief Destructor - restores original console code page
    */
    ~ConsoleUTF8();

  private:
    unsigned int m_original_cp; ///< Saved original console code page
};
