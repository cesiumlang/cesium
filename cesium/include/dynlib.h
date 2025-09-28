/**
@brief Cross-platform dynamic library loading utilities
*/
#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
  #include "win32.h"
  using LibraryHandle = win32::HMODULE;  ///< Windows library handle type
#else
  using LibraryHandle = void*;           ///< POSIX library handle type
#endif

namespace dynlib {

/**
@brief Cross-platform dynamic library handle wrapper

RAII wrapper for dynamic library handles that provides safe loading,
function resolution, and automatic cleanup on destruction.
*/
class Library {
  private:
    LibraryHandle handle_;  ///< Platform-specific library handle
    std::string path_;      ///< Path to the loaded library file

  public:
    /**
    @brief Default constructor for empty library handle
    */
    Library();

    /**
    @brief Construct from existing library handle
    @param handle Platform-specific library handle
    @param path Path to library file (optional)
    */
    explicit Library(LibraryHandle handle, const std::string& path = "");

    /**
    @brief Destructor - automatically closes library handle
    */
    ~Library();

    // Move semantics for safe transfer of ownership
    Library(Library&& other) noexcept;
    Library& operator=(Library&& other) noexcept;

    // Disable copy semantics to prevent double-free
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;

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
    FuncType getFunction(const std::string& name) const {
      return reinterpret_cast<FuncType>(getFunctionRaw(name));
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
    void* getFunctionRaw(const std::string& name) const;
};

/**
@brief Load dynamic library from file path
@param path Full or relative path to library file
@return Library wrapper or invalid library on failure
*/
Library loadLib(const std::string& path);

/**
@brief Load library by base name from multiple search paths
@param baseName Base name without platform-specific prefix/suffix
@param searchPaths Vector of directories to search (uses defaults if empty)
@return Library wrapper or invalid library if not found
*/
Library loadLibraryFromPaths(const std::string& baseName, const std::vector<std::string>& searchPaths = {});

/**
@brief Get last error message from dynamic library system
@return Error description string
*/
std::string getLastError();

/**
@brief Resolve platform-specific library filename from base name
@param baseName Base library name (e.g., "tree-sitter-cpp")
@return Platform-specific filename (e.g., "tree-sitter-cpp.dll" on Windows, "libtree-sitter-cpp.so" on Linux)
*/
std::string resolvePlatformLibraryName(const std::string& baseName);

/**
@brief Find library file that exists in directory using naming conventions
@param baseName Base library name
@param searchDir Directory to search in (defaults to current directory)
@return Full path to found library file or empty string if not found
*/
std::string findLibraryFile(const std::string& baseName, const std::string& searchDir = ".");

} // namespace dynlib
