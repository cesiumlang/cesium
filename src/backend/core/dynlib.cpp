/**
@brief Cross-platform dynamic library loading implementation
*/
#include <backend/core/dynlib.h>
#include <backend/core/cli_utils.h>
#include <filesystem>

#ifdef _WIN32
  #include <backend/core/win32.h>
#else
  #include <dlfcn.h>
#endif

namespace dynlib {

// DynLib class implementations
DynLib::DynLib() : handle_(nullptr) {}

DynLib::DynLib(DynLibHandle handle, const std::string& path)
  : handle_(handle), path_(path) {}

DynLib::~DynLib() {
  close();
}

DynLib::DynLib(DynLib&& other) noexcept
  : handle_(other.handle_), path_(std::move(other.path_)) {
  other.handle_ = nullptr;
}

DynLib& DynLib::operator=(DynLib&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = other.handle_;
    path_ = std::move(other.path_);
    other.handle_ = nullptr;
  }
  return *this;
}

bool DynLib::isValid() const {
  return handle_ != nullptr;
}

const std::string& DynLib::getPath() const {
  return path_;
}

void* DynLib::getFuncRaw(const std::string& name) const {
  CLILogger::debug("DynLib::getFuncRaw: Attempting to get function '" + name + "' from library: " + path_);
  
  if (!handle_) {
    CLILogger::debug("DynLib::getFuncRaw: Library handle is null, cannot get function: " + name);
    return nullptr;
  }

  #ifdef _WIN32
    void* func = reinterpret_cast<void*>(GetProcAddress(handle_, name.c_str()));
  #else
    void* func = dlsym(handle_, name.c_str());
  #endif
  
  if (func) {
    CLILogger::debug("DynLib::getFuncRaw: Successfully found function '" + name + "' in library: " + path_);
  } else {
    std::string error = getLastDynLibError();
    CLILogger::debug("DynLib::getFuncRaw: Failed to find function '" + name + "' in library " + path_ + ": " + error);
  }
  
  return func;
}

void DynLib::close() {
  if (handle_) {
    #ifdef _WIN32
      FreeLibrary(handle_);
    #else
      dlclose(handle_);
    #endif
    handle_ = nullptr;
  }
}

DynLib::operator bool() const {
  return isValid();
}

// Helper function to get platform-specific extension
std::string getPlatformExt() {
  #ifdef _WIN32
    return ".dll";
  #elif defined(__APPLE__)
    return ".dylib";
  #else
    return ".so";
  #endif
}

// Helper function to replace extension in path
std::string replaceExt(const std::string& path, const std::string& newExt) {
  size_t lastDot = path.find_last_of('.');
  if (lastDot != std::string::npos) {
    return path.substr(0, lastDot) + newExt;
  }
  return path + newExt;
}

// Free function implementations
DynLib loadDynLib(const std::string& path) {
  CLILogger::debug("loadDynLib: Attempting to load library: " + path);
  
  // First, try the exact path as given
  CLILogger::debug("loadDynLib: Trying exact path: " + path);
  #ifdef _WIN32
    DynLibHandle handle = LoadLibraryW(utf8::widen(path).c_str());
  #else
    DynLibHandle handle = dlopen(path.c_str(), RTLD_LAZY);
  #endif

  if (handle) {
    CLILogger::debug("loadDynLib: Successfully loaded library from exact path: " + path);
    return DynLib(handle, path);
  }
  
  std::string exactPathError = getLastDynLibError();
  CLILogger::debug("loadDynLib: Exact path failed: " + exactPathError);

  // If that failed, try with platform-specific extension
  std::string platformExt = getPlatformExt();
  std::string platformPath = replaceExt(path, platformExt);

  // Only try the platform path if it's different from the original
  if (platformPath != path) {
    CLILogger::debug("loadDynLib: Trying platform-specific path: " + platformPath);
    #ifdef _WIN32
      handle = LoadLibraryW(utf8::widen(platformPath).c_str());
    #else
      handle = dlopen(platformPath.c_str(), RTLD_LAZY);
    #endif

    if (handle) {
      CLILogger::debug("loadDynLib: Successfully loaded library from platform path: " + platformPath);
      return DynLib(handle, platformPath);
    }
    
    std::string platformPathError = getLastDynLibError();
    CLILogger::debug("loadDynLib: Platform path failed: " + platformPathError);
  } else {
    CLILogger::debug("loadDynLib: Skipping platform path (same as original): " + platformPath);
  }

  // Return invalid library if both attempts failed
  CLILogger::debug("loadDynLib: All attempts failed for library: " + path);
  return DynLib(nullptr, path);
}

DynLib loadDynLibFromPaths(const std::string& baseName, const std::vector<std::string>& searchPaths) {
  CLILogger::debug("loadDynLibFromPaths: Searching for library '" + baseName + "' in " + std::to_string(searchPaths.size()) + " paths");
  
  std::vector<std::string> paths = searchPaths;

  // Add default search paths if none provided
  if (paths.empty()) {
    CLILogger::debug("loadDynLibFromPaths: Using default search paths (no paths provided)");
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
    CLILogger::debug("loadDynLibFromPaths: Searching in directory: " + searchDir);
    std::string resolved_name = findDynLibFile(baseName, searchDir);
    if (resolved_name != baseName) { // Found something different
      std::filesystem::path full_path = std::filesystem::path(searchDir) / resolved_name;
      CLILogger::debug("loadDynLibFromPaths: Found potential match: " + full_path.string());
      DynLib lib = loadDynLib(full_path.string());
      if (lib.isValid()) {
        CLILogger::debug("loadDynLibFromPaths: Successfully loaded library from: " + full_path.string());
        return lib;
      }
      CLILogger::debug("loadDynLibFromPaths: Failed to load found match: " + full_path.string());
    } else {
      CLILogger::debug("loadDynLibFromPaths: No matches found in directory: " + searchDir);
    }
  }

  // Fallback: try loading with platform-specific name directly
  std::string platform_name = resolvePlatformDynLibName(baseName);
  CLILogger::debug("loadDynLibFromPaths: Trying fallback platform name: " + platform_name);
  DynLib lib = loadDynLib(platform_name);
  if (lib.isValid()) {
    CLILogger::debug("loadDynLibFromPaths: Successfully loaded library from fallback: " + platform_name);
    return lib;
  }
  CLILogger::debug("loadDynLibFromPaths: Fallback failed: " + platform_name);

  CLILogger::debug("loadDynLibFromPaths: All search attempts failed for library: " + baseName);
  return DynLib(); // Return invalid library
}

std::string getLastDynLibError() {
  #ifdef _WIN32
    DWORD error = GetLastError();
    if (error == 0) return "";
    LPTSTR msgbuf = nullptr;
    size_t size = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&msgbuf, 0, nullptr
    );
    return utf8::narrow(msgbuf, size);
  #else
    const char* error = dlerror();
    return error ? std::string(error) : "";
  #endif
}

std::string resolvePlatformDynLibName(const std::string& baseName) {
  // First, strip any existing platform-specific extension to get the base name
  std::string cleanBase = baseName;
  std::string targetExt = getPlatformExt();
  
  // Remove any existing dynamic library extensions (.dll, .so, .dylib)
  if (cleanBase.ends_with(".dll")) {
    cleanBase = cleanBase.substr(0, cleanBase.length() - 4);
  } else if (cleanBase.ends_with(".so")) {
    cleanBase = cleanBase.substr(0, cleanBase.length() - 3);
  } else if (cleanBase.ends_with(".dylib")) {
    cleanBase = cleanBase.substr(0, cleanBase.length() - 6);
  }
  
  #ifdef _WIN32
    // On Windows, just add .dll extension
    return cleanBase + targetExt;
  #elif defined(__APPLE__)
    // On macOS, just add .dylib extension
    return cleanBase + targetExt;
  #else
    // On Linux/POSIX, add lib prefix if not present and .so extension
    if (cleanBase.starts_with("lib")) {
      return cleanBase + targetExt;
    }
    return "lib" + cleanBase + targetExt;
  #endif
}

std::string findDynLibFile(const std::string& baseName, const std::string& searchDir) {
  std::vector<std::string> candidates;

  #ifdef _WIN32
    // On Windows, try various combinations
    candidates = {
      baseName,
      baseName + ".dll",
      "lib" + baseName + ".dll",
      // baseName + ".so",
      // "lib" + baseName + ".so"
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

std::vector<std::string> getSystemSearchPaths() {
  std::vector<std::string> paths = {
    "." // Current directory always first
  };
  
  #ifdef _WIN32
    // Windows-specific search paths
    paths.push_back("build/bin");
    paths.push_back("bin");
    // Could add more Windows paths here like System32, etc.
  #elif defined(__APPLE__)
    // macOS-specific search paths
    paths.push_back("build/bin");
    paths.push_back("bin");
    paths.push_back("/usr/local/lib");
    paths.push_back("/usr/lib");
    paths.push_back("/System/Library/Frameworks");
  #else
    // Linux/POSIX search paths
    paths.push_back("build/bin");
    paths.push_back("bin");
    paths.push_back("/usr/local/lib");
    paths.push_back("/usr/lib");
    paths.push_back("/lib");
  #endif
  
  return paths;
}

DynLib loadDynLibFromConfig(const std::string& libraryPath, const std::string& configFilePath) {
  CLILogger::debug("loadDynLibFromConfig: Attempting to load library '" + libraryPath + "' with config '" + configFilePath + "'");
  
  std::filesystem::path libPath(libraryPath);
  std::filesystem::path configPath(configFilePath);
  
  // 1. If absolute path, try it directly
  if (libPath.is_absolute()) {
    CLILogger::debug("loadDynLibFromConfig: Trying absolute path strategy");
    std::string platformPath = resolvePlatformDynLibName(libraryPath);
    CLILogger::debug("loadDynLibFromConfig: Trying platform-resolved path: " + platformPath);
    DynLib lib = loadDynLib(platformPath);
    if (lib.isValid()) {
      CLILogger::debug("loadDynLibFromConfig: Successfully loaded library from platform path: " + platformPath);
      return lib;
    }
    CLILogger::debug("loadDynLibFromConfig: Platform path failed: " + getLastDynLibError());
    
    // Also try original path in case it's already platform-specific
    CLILogger::debug("loadDynLibFromConfig: Trying original absolute path: " + libraryPath);
    lib = loadDynLib(libraryPath);
    if (lib.isValid()) {
      CLILogger::debug("loadDynLibFromConfig: Successfully loaded library from original path: " + libraryPath);
      return lib;
    }
    CLILogger::debug("loadDynLibFromConfig: Original absolute path failed: " + getLastDynLibError());
  }
  
  // 2. Try relative to config file directory
  if (!configFilePath.empty() && !configPath.parent_path().empty()) {
    CLILogger::debug("loadDynLibFromConfig: Trying relative to config file directory strategy");
    std::filesystem::path relativePath = configPath.parent_path() / libPath;
    std::string platformPath = resolvePlatformDynLibName(relativePath.string());
    CLILogger::debug("loadDynLibFromConfig: Trying config-relative platform path: " + platformPath);
    DynLib lib = loadDynLib(platformPath);
    if (lib.isValid()) {
      CLILogger::debug("loadDynLibFromConfig: Successfully loaded library from config-relative platform path: " + platformPath);
      return lib;
    }
    CLILogger::debug("loadDynLibFromConfig: Config-relative platform path failed: " + getLastDynLibError());
    
    // Also try original path
    CLILogger::debug("loadDynLibFromConfig: Trying config-relative original path: " + relativePath.string());
    lib = loadDynLib(relativePath.string());
    if (lib.isValid()) {
      CLILogger::debug("loadDynLibFromConfig: Successfully loaded library from config-relative original path: " + relativePath.string());
      return lib;
    }
    CLILogger::debug("loadDynLibFromConfig: Config-relative original path failed: " + getLastDynLibError());
  } else {
    CLILogger::debug("loadDynLibFromConfig: Skipping config-relative strategy (empty config path or parent)");
  }
  
  // 3. Try relative to current working directory
  CLILogger::debug("loadDynLibFromConfig: Trying current working directory strategy");
  std::string platformPath = resolvePlatformDynLibName(libraryPath);
  CLILogger::debug("loadDynLibFromConfig: Trying CWD platform path: " + platformPath);
  DynLib lib = loadDynLib(platformPath);
  if (lib.isValid()) {
    CLILogger::debug("loadDynLibFromConfig: Successfully loaded library from CWD platform path: " + platformPath);
    return lib;
  }
  CLILogger::debug("loadDynLibFromConfig: CWD platform path failed: " + getLastDynLibError());
  
  CLILogger::debug("loadDynLibFromConfig: Trying CWD original path: " + libraryPath);
  lib = loadDynLib(libraryPath);
  if (lib.isValid()) {
    CLILogger::debug("loadDynLibFromConfig: Successfully loaded library from CWD original path: " + libraryPath);
    return lib;
  }
  CLILogger::debug("loadDynLibFromConfig: CWD original path failed: " + getLastDynLibError());
  
  // 4. Search using filename only in prioritized search paths
  std::string filename = libPath.filename().string();
  CLILogger::debug("loadDynLibFromConfig: Trying filename-only search strategy with filename: " + filename);
  
  // First try config file directory with filename only
  if (!configFilePath.empty() && !configPath.parent_path().empty()) {
    CLILogger::debug("loadDynLibFromConfig: Searching in config directory: " + configPath.parent_path().string());
    std::string found = findDynLibFile(filename, configPath.parent_path().string());
    if (found != filename) { // Found something different
      std::filesystem::path fullPath = configPath.parent_path() / found;
      CLILogger::debug("loadDynLibFromConfig: Found potential match in config dir: " + fullPath.string());
      lib = loadDynLib(fullPath.string());
      if (lib.isValid()) {
        CLILogger::debug("loadDynLibFromConfig: Successfully loaded library from config dir search: " + fullPath.string());
        return lib;
      }
      CLILogger::debug("loadDynLibFromConfig: Config dir match failed: " + getLastDynLibError());
    } else {
      CLILogger::debug("loadDynLibFromConfig: No matches found in config directory");
    }
  } else {
    CLILogger::debug("loadDynLibFromConfig: Skipping config directory search (empty config path)");
  }
  
  // Then search in system paths
  std::vector<std::string> searchPaths = getSystemSearchPaths();
  CLILogger::debug("loadDynLibFromConfig: Searching in " + std::to_string(searchPaths.size()) + " system paths");
  for (const auto& searchDir : searchPaths) {
    CLILogger::debug("loadDynLibFromConfig: Searching in system directory: " + searchDir);
    std::string found = findDynLibFile(filename, searchDir);
    if (found != filename) { // Found something different
      std::filesystem::path fullPath = std::filesystem::path(searchDir) / found;
      CLILogger::debug("loadDynLibFromConfig: Found potential match in system dir: " + fullPath.string());
      lib = loadDynLib(fullPath.string());
      if (lib.isValid()) {
        CLILogger::debug("loadDynLibFromConfig: Successfully loaded library from system dir search: " + fullPath.string());
        return lib;
      }
      CLILogger::debug("loadDynLibFromConfig: System dir match failed: " + getLastDynLibError());
    } else {
      CLILogger::debug("loadDynLibFromConfig: No matches found in system directory: " + searchDir);
    }
  }
  
  // 5. Final fallback: try using loadDynLibFromPaths for platform resolution
  CLILogger::debug("loadDynLibFromConfig: Trying final fallback with loadDynLibFromPaths");
  DynLib result = loadDynLibFromPaths(filename, searchPaths);
  
  if (!result.isValid()) {
    CLILogger::error("loadDynLibFromConfig: Failed to load library '" + libraryPath + "' after trying all strategies");
    CLILogger::debug("loadDynLibFromConfig: All library loading strategies exhausted for: " + libraryPath);
  }
  
  return result;
}

} // namespace dynlib
