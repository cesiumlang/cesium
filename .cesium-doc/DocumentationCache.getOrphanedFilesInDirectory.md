---
type: function
namespace: DocumentationCache
name: getOrphanedFilesInDirectory
full_name: DocumentationCache::getOrphanedFilesInDirectory
start_line: 104
end_line: 104
file: include/backend\doc\cache.h
return_type: std::vector<std::string>
---

# getOrphanedFilesInDirectory

*function in DocumentationCache*

## Signature

```cpp
std::vector<std::string> getOrphanedFilesInDirectory()
```

## Returns

`std::vector<std::string>`

*(No description available)*

## Documentation

/**
    @brief Get list of orphaned files in extract directory (files not in cache)
    @param extract_dir Directory containing generated files
    @return Vector of file paths that are not tracked by cache
    */

## Source

**File:** `include/backend\doc\cache.h`

**Lines:** 104-104
