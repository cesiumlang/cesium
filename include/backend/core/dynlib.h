/**
@brief Cross-platform dynamic library loading utilities
*/
#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
  #include <backend/core/win32.h>
  using DynLibHandle = HMODULE;  ///< Windows library handle type
#else
  using DynLibHandle = void*;           ///< POSIX library handle type
#endif

namespace dynlib {

/**
@brief Cross-platform dynamic library handle wrapper

RAII wrapper for dynamic library handles that provides safe loading,
function resolution, and automatic cleanup on destruction.
*/
class DynLib {
  private:
    DynLibHandle handle_;  ///< Platform-specific library handle
    std::string path_;      ///< Path to the loaded library file

  public:
    /**
    @brief Default constructor for empty library handle
    */
    DynLib();

    /**
    @brief Construct from existing library handle
    @param handle Platform-specific library handle
    @param path Path to library file (optional)
    */
    explicit DynLib(DynLibHandle handle, const std::string& path = "");

    /**
    @brief Destructor - automatically closes library handle
    */
    ~DynLib();

    // Move semantics for safe transfer of ownership
    DynLib(DynLib&& other) noexcept;
    DynLib& operator=(DynLib&& other) noexcept;

    // Disable copy semantics to prevent double-free
    DynLib(const DynLib&) = delete;
    DynLib& operator=(const DynLib&) = delete;

    /**
    @brief Check if library handle is valid
    */
    bool isValid() const;

    /**
    @brief Get path to loaded library file
    */
    const std::string& getPath() const;

    /**
    @brief Get function pointer from library
    @tparam FuncType Function pointer type to cast to
    @param name Symbol name to resolve
    @return Function pointer or nullptr if not found
    */
    template<typename FuncType>
    FuncType getFunc(const std::string& name) const {
      return reinterpret_cast<FuncType>(getFuncRaw(name));
    }

    /**
    @brief Explicitly close library handle
    */
    void close();

    /**
    @brief Boolean conversion for validity checking
    */
    operator bool() const;

  private:
    /**
    @brief Get raw function pointer from library
    */
    void* getFuncRaw(const std::string& name) const;
};

/**
@brief Load dynamic library from file path
@param path Full or relative path to library file
@return DynLib wrapper or invalid library on failure
*/
DynLib loadDynLib(const std::string& path);

/**
@brief Load library by base name from multiple search paths
@param baseName Base name without platform-specific prefix/suffix
@param searchPaths Vector of directories to search (uses defaults if empty)
@return DynLib wrapper or invalid library if not found
*/
DynLib loadDynLibFromPaths(const std::string& baseName, const std::vector<std::string>& searchPaths = {});

/**
@brief Get last error message from dynamic library system
@return Error description string
*/
std::string getLastDynLibError();

/**
@brief Resolve platform-specific library filename from base name
@param baseName Base library name (e.g., "tree-sitter-cpp")
@return Platform-specific filename (e.g., "tree-sitter-cpp.dll" on Windows, "libtree-sitter-cpp.so" on Linux)
*/
std::string resolvePlatformDynLibName(const std::string& baseName);

/**
@brief Find library file that exists in directory using naming conventions
@param baseName Base library name
@param searchDir Directory to search in (defaults to current directory)
@return Full path to found library file or empty string if not found
*/
std::string findDynLibFile(const std::string& baseName, const std::string& searchDir = ".");

/**
@brief Get standard system search paths for dynamic libraries
@return Vector of search paths in priority order
*/
std::vector<std::string> getSystemSearchPaths();

/**
@brief Load library using config-based priority search strategy
@param libraryPath Library path from config (can be absolute, relative, or basename)
@param configFilePath Path to the config file (for relative path resolution)
@return DynLib wrapper or invalid library if not found
*/
DynLib loadDynLibFromConfig(const std::string& libraryPath, const std::string& configFilePath = "");

} // namespace dynlib
