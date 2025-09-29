#include <backend/core/dynlib.h>
#include "../testfrmwk/simple_test.h"

/**
 * @brief Tests platform-specific dynamic library name resolution
 * 
 * This test validates the requirement that the dynamic library system must handle
 * cross-platform library naming conventions by automatically converting library
 * names to the appropriate platform-specific extension (.dll on Windows, .dylib 
 * on macOS, .so on Linux).
 * 
 * Testing rationale: When users specify library names in config files, they often
 * use Unix-style naming (.so) regardless of platform. The system must transparently
 * convert these to the correct platform extension to enable cross-platform configs.
 * 
 * Requirements tested:
 * - .so files should be converted to platform-appropriate extension
 * - .dylib files should be converted to platform-appropriate extension  
 * - .dll files should be converted to platform-appropriate extension
 * - Base names without extensions should get platform-appropriate extension
 * - lib prefixes should be preserved during conversion
 * - Conversion should be consistent across different input patterns
 */
void test_resolve_platform_name_windows() {
  // Test various inputs that should resolve to .dll on Windows
  
  // Test .so to .dll conversion
  std::string result1 = dynlib::resolvePlatformDynLibName("tree-sitter-cpp.so");
  std::string result2 = dynlib::resolvePlatformDynLibName("libmylibrary.so");
  std::string result3 = dynlib::resolvePlatformDynLibName("simple.so");
  
  // Test .dylib to .dll conversion  
  std::string result4 = dynlib::resolvePlatformDynLibName("tree-sitter-cpp.dylib");
  std::string result5 = dynlib::resolvePlatformDynLibName("libmylibrary.dylib");
  
  // Test .dll passthrough
  std::string result6 = dynlib::resolvePlatformDynLibName("tree-sitter-cpp.dll");
  std::string result7 = dynlib::resolvePlatformDynLibName("mylibrary.dll");
  
  // Test base name without extension
  std::string result8 = dynlib::resolvePlatformDynLibName("tree-sitter-cpp");
  std::string result9 = dynlib::resolvePlatformDynLibName("mylibrary");

#ifdef _WIN32
  // On Windows, all should resolve to .dll
  TEST_ASSERT_EQ(result1, "tree-sitter-cpp.dll", "so_to_dll_conversion");
  TEST_ASSERT_EQ(result2, "libmylibrary.dll", "lib_so_to_dll_conversion");
  TEST_ASSERT_EQ(result3, "simple.dll", "simple_so_to_dll_conversion");
  
  TEST_ASSERT_EQ(result4, "tree-sitter-cpp.dll", "dylib_to_dll_conversion");
  TEST_ASSERT_EQ(result5, "libmylibrary.dll", "lib_dylib_to_dll_conversion");
  
  TEST_ASSERT_EQ(result6, "tree-sitter-cpp.dll", "dll_passthrough");
  TEST_ASSERT_EQ(result7, "mylibrary.dll", "simple_dll_passthrough");
  
  TEST_ASSERT_EQ(result8, "tree-sitter-cpp.dll", "base_name_to_dll");
  TEST_ASSERT_EQ(result9, "mylibrary.dll", "simple_base_to_dll");
#elif defined(__APPLE__)
  // On macOS, all should resolve to .dylib
  TEST_ASSERT_EQ(result1, "tree-sitter-cpp.dylib", "so_to_dylib_conversion");
  TEST_ASSERT_EQ(result2, "libmylibrary.dylib", "lib_so_to_dylib_conversion");
  TEST_ASSERT_EQ(result3, "simple.dylib", "simple_so_to_dylib_conversion");
  
  TEST_ASSERT_EQ(result4, "tree-sitter-cpp.dylib", "dylib_passthrough");
  TEST_ASSERT_EQ(result5, "libmylibrary.dylib", "lib_dylib_passthrough");
  
  TEST_ASSERT_EQ(result6, "tree-sitter-cpp.dylib", "dll_to_dylib_conversion");
  TEST_ASSERT_EQ(result7, "mylibrary.dylib", "simple_dll_to_dylib_conversion");
  
  TEST_ASSERT_EQ(result8, "tree-sitter-cpp.dylib", "base_name_to_dylib");
  TEST_ASSERT_EQ(result9, "mylibrary.dylib", "simple_base_to_dylib");
#else
  // On Linux/POSIX, should resolve to .so with lib prefix where appropriate
  TEST_ASSERT_EQ(result1, "libtree-sitter-cpp.so", "so_to_lib_so_conversion");
  TEST_ASSERT_EQ(result2, "libmylibrary.so", "lib_so_passthrough");
  TEST_ASSERT_EQ(result3, "libsimple.so", "simple_to_lib_so_conversion");
  
  TEST_ASSERT_EQ(result4, "libtree-sitter-cpp.so", "dylib_to_lib_so_conversion");
  TEST_ASSERT_EQ(result5, "libmylibrary.so", "lib_dylib_to_so_conversion");
  
  TEST_ASSERT_EQ(result6, "libtree-sitter-cpp.so", "dll_to_lib_so_conversion");
  TEST_ASSERT_EQ(result7, "libmylibrary.so", "simple_dll_to_lib_so_conversion");
  
  TEST_ASSERT_EQ(result8, "libtree-sitter-cpp.so", "base_name_to_lib_so");
  TEST_ASSERT_EQ(result9, "libmylibrary.so", "simple_base_to_lib_so");
#endif
}

void test_resolve_platform_name_paths() {
  // Test full paths with platform-specific extensions
  
  std::string result1 = dynlib::resolvePlatformDynLibName("../build/bin/tree-sitter-cpp.so");
  std::string result2 = dynlib::resolvePlatformDynLibName("/usr/lib/libmylibrary.so");
  std::string result3 = dynlib::resolvePlatformDynLibName("C:\\Windows\\System32\\kernel32.dll");
  std::string result4 = dynlib::resolvePlatformDynLibName("/System/Library/Frameworks/Foundation.dylib");

#ifdef _WIN32
  TEST_ASSERT_EQ(result1, "../build/bin/tree-sitter-cpp.dll", "path_so_to_dll_conversion");
  TEST_ASSERT_EQ(result2, "/usr/lib/libmylibrary.dll", "unix_path_so_to_dll_conversion");
  TEST_ASSERT_EQ(result3, "C:\\Windows\\System32\\kernel32.dll", "windows_path_dll_passthrough");
  TEST_ASSERT_EQ(result4, "/System/Library/Frameworks/Foundation.dll", "mac_path_dylib_to_dll_conversion");
#elif defined(__APPLE__)
  TEST_ASSERT_EQ(result1, "../build/bin/tree-sitter-cpp.dylib", "path_so_to_dylib_conversion");
  TEST_ASSERT_EQ(result2, "/usr/lib/libmylibrary.dylib", "unix_path_so_to_dylib_conversion");
  TEST_ASSERT_EQ(result3, "C:\\Windows\\System32\\kernel32.dylib", "windows_path_dll_to_dylib_conversion");
  TEST_ASSERT_EQ(result4, "/System/Library/Frameworks/Foundation.dylib", "mac_path_dylib_passthrough");
#else
  TEST_ASSERT_EQ(result1, "../build/bin/libtree-sitter-cpp.so", "path_so_to_lib_so_conversion");
  TEST_ASSERT_EQ(result2, "/usr/lib/libmylibrary.so", "unix_path_so_passthrough");
  TEST_ASSERT_EQ(result3, "C:\\Windows\\System32\\libkernel32.so", "windows_path_dll_to_lib_so_conversion");
  TEST_ASSERT_EQ(result4, "/System/Library/Frameworks/libFoundation.so", "mac_path_dylib_to_lib_so_conversion");
#endif
}

void test_resolve_platform_name_edge_cases() {
  // Test edge cases
  
  std::string result1 = dynlib::resolvePlatformDynLibName("");
  std::string result2 = dynlib::resolvePlatformDynLibName("no-extension");
  std::string result3 = dynlib::resolvePlatformDynLibName("already.has.dots");
  std::string result4 = dynlib::resolvePlatformDynLibName("multiple.so.so");
  std::string result5 = dynlib::resolvePlatformDynLibName("lib");
  std::string result6 = dynlib::resolvePlatformDynLibName("lib.so");

#ifdef _WIN32
  TEST_ASSERT_EQ(result1, ".dll", "empty_name_to_dll");
  TEST_ASSERT_EQ(result2, "no-extension.dll", "no_extension_to_dll");
  TEST_ASSERT_EQ(result3, "already.has.dots.dll", "dots_in_name_to_dll");
  TEST_ASSERT_EQ(result4, "multiple.so.dll", "multiple_so_to_dll");
  TEST_ASSERT_EQ(result5, "lib.dll", "lib_only_to_dll");
  TEST_ASSERT_EQ(result6, "lib.dll", "lib_so_to_dll");
#elif defined(__APPLE__)
  TEST_ASSERT_EQ(result1, ".dylib", "empty_name_to_dylib");
  TEST_ASSERT_EQ(result2, "no-extension.dylib", "no_extension_to_dylib");
  TEST_ASSERT_EQ(result3, "already.has.dots.dylib", "dots_in_name_to_dylib");
  TEST_ASSERT_EQ(result4, "multiple.so.dylib", "multiple_so_to_dylib");
  TEST_ASSERT_EQ(result5, "lib.dylib", "lib_only_to_dylib");
  TEST_ASSERT_EQ(result6, "lib.dylib", "lib_so_to_dylib");
#else
  TEST_ASSERT_EQ(result1, "lib.so", "empty_name_to_lib_so");
  TEST_ASSERT_EQ(result2, "libno-extension.so", "no_extension_to_lib_so");
  TEST_ASSERT_EQ(result3, "libalready.has.dots.so", "dots_in_name_to_lib_so");
  TEST_ASSERT_EQ(result4, "libmultiple.so.so", "multiple_so_to_lib_so");
  TEST_ASSERT_EQ(result5, "liblib.so", "lib_only_to_lib_so");
  TEST_ASSERT_EQ(result6, "lib.so", "lib_so_passthrough");
#endif
}

void test_platform_extension_getter() {
  // Test the getPlatformExt function (though it's not exposed in header, we can test through resolvePlatformDynLibName)
  
  std::string base_test = dynlib::resolvePlatformDynLibName("test");
  
#ifdef _WIN32
  TEST_ASSERT_TRUE(base_test.ends_with(".dll"), "platform_ext_is_dll");
#elif defined(__APPLE__)
  TEST_ASSERT_TRUE(base_test.ends_with(".dylib"), "platform_ext_is_dylib");
#else
  TEST_ASSERT_TRUE(base_test.ends_with(".so"), "platform_ext_is_so");
#endif
}

void run_dynlib_platform_tests() {
  test_resolve_platform_name_windows();
  test_resolve_platform_name_paths();
  test_resolve_platform_name_edge_cases();
  test_platform_extension_getter();
}