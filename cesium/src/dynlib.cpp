/**
 * @file dynlib.cpp
 * @brief Cross-platform dynamic library loading implementation
 */
#include "dynlib.h"
#include <filesystem>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif

namespace dynlib {

// Library class implementations
Library::Library() : handle_(nullptr) {}

Library::Library(LibraryHandle handle, const std::string& path)
  : handle_(handle), path_(path) {}

Library::~Library() {
  close();
}

Library::Library(Library&& other) noexcept
  : handle_(other.handle_), path_(std::move(other.path_)) {
  other.handle_ = nullptr;
}

Library& Library::operator=(Library&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = other.handle_;
    path_ = std::move(other.path_);
    other.handle_ = nullptr;
  }
  return *this;
}

bool Library::isValid() const {
  return handle_ != nullptr;
}

const std::string& Library::getPath() const {
  return path_;
}

void* Library::getFunctionRaw(const std::string& name) const {
  if (!handle_) return nullptr;

  #ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(handle_, name.c_str()));
  #else
    return dlsym(handle_, name.c_str());
  #endif
}

void Library::close() {
  if (handle_) {
    #ifdef _WIN32
      FreeLibrary(handle_);
    #else
      dlclose(handle_);
    #endif
    handle_ = nullptr;
  }
}

Library::operator bool() const {
  return isValid();
}

// Helper function to get platform-specific extension
std::string getPlatformExtension() {
  #ifdef _WIN32
    return ".dll";
  #elif defined(__APPLE__)
    return ".dylib";
  #else
    return ".so";
  #endif
}

// Helper function to replace extension in path
std::string replaceExtension(const std::string& path, const std::string& newExt) {
  size_t lastDot = path.find_last_of('.');
  if (lastDot != std::string::npos) {
    return path.substr(0, lastDot) + newExt;
  }
  return path + newExt;
}

// Free function implementations
Library loadLib(const std::string& path) {
  // First, try the exact path as given
  #ifdef _WIN32
    LibraryHandle handle = LoadLibraryA(path.c_str());
  #else
    LibraryHandle handle = dlopen(path.c_str(), RTLD_LAZY);
  #endif

  if (handle) {
    return Library(handle, path);
  }

  // If that failed, try with platform-specific extension
  std::string platformExt = getPlatformExtension();
  std::string platformPath = replaceExtension(path, platformExt);

  // Only try the platform path if it's different from the original
  if (platformPath != path) {
    #ifdef _WIN32
      handle = LoadLibraryA(platformPath.c_str());
    #else
      handle = dlopen(platformPath.c_str(), RTLD_LAZY);
    #endif

    if (handle) {
      return Library(handle, platformPath);
    }
  }

  // Return invalid library if both attempts failed
  return Library(nullptr, path);
}

Library loadLibraryFromPaths(const std::string& baseName, const std::vector<std::string>& searchPaths) {
  std::vector<std::string> paths = searchPaths;

  // Add default search paths if none provided
  if (paths.empty()) {
    paths = {
      ".",
      #ifndef _WIN32
        "/usr/local/lib",
        "/usr/lib"
      #endif
    };
  }

  // Try to find the library in each search path
  for (const auto& searchDir : paths) {
    std::string resolved_name = findLibraryFile(baseName, searchDir);
    if (resolved_name != baseName) { // Found something different
      std::filesystem::path full_path = std::filesystem::path(searchDir) / resolved_name;
      Library lib = loadLib(full_path.string());
      if (lib.isValid()) {
        return lib;
      }
    }
  }

  // Fallback: try loading with platform-specific name directly
  std::string platform_name = resolvePlatformLibraryName(baseName);
  Library lib = loadLib(platform_name);
  if (lib.isValid()) {
    return lib;
  }

  return Library(); // Return invalid library
}

std::string getLastError() {
  #ifdef _WIN32
    DWORD error = GetLastError();
    if (error == 0) return "";

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&messageBuffer, 0, nullptr);

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
  #else
    const char* error = dlerror();
    return error ? std::string(error) : "";
  #endif
}

std::string resolvePlatformLibraryName(const std::string& baseName) {
#ifdef _WIN32
  // On Windows, prefer .dll but also consider .so for compatibility
  if (baseName.ends_with(".dll") || baseName.ends_with(".so")) {
    return baseName; // Already has extension
  }
  return baseName + ".dll";
#else
  // On POSIX, use lib prefix and .so extension if not already present
  if (baseName.ends_with(".so")) {
    return baseName; // Already has extension
  }
  if (baseName.starts_with("lib")) {
    return baseName + ".so";
  }
  return "lib" + baseName + ".so";
#endif
}

std::string findLibraryFile(const std::string& baseName, const std::string& searchDir) {
  std::vector<std::string> candidates;
  
#ifdef _WIN32
  // On Windows, try various combinations
  candidates = {
    baseName,
    baseName + ".dll",
    "lib" + baseName + ".dll",
    baseName + ".so",
    "lib" + baseName + ".so"
  };
#else
  // On POSIX, try standard naming conventions
  candidates = {
    baseName,
    "lib" + baseName + ".so",
    baseName + ".so"
  };
#endif
  
  for (const auto& candidate : candidates) {
    std::filesystem::path full_path = std::filesystem::path(searchDir) / candidate;
    if (std::filesystem::exists(full_path)) {
      return candidate;
    }
  }
  
  return baseName; // Return original if nothing found
}

} // namespace dynlib
