---
type: function
name: resolvePlatformDynLibName
full_name: resolvePlatformDynLibName
start_line: 119
end_line: 119
file: include/backend\core\dynlib.h
return_type: std::string
---

# resolvePlatformDynLibName

*function*

## Signature

```cpp
std::string resolvePlatformDynLibName()
```

## Returns

`std::string`

*(No description available)*

## Documentation

/**
@brief Resolve platform-specific library filename from base name
@param baseName Base library name (e.g., "tree-sitter-cpp")
@return Platform-specific filename (e.g., "tree-sitter-cpp.dll" on Windows, "libtree-sitter-cpp.so" on Linux)
*/

## Source

**File:** `include/backend\core\dynlib.h`

**Lines:** 119-119
