---
type: function
namespace: DocumentationCache
name: pruneOrphanedFiles
full_name: DocumentationCache::pruneOrphanedFiles
start_line: 112
end_line: 112
file: include/backend\doc\cache.h
return_type: size_t
---

# pruneOrphanedFiles

*function in DocumentationCache*

## Signature

```cpp
size_t pruneOrphanedFiles()
```

## Returns

`size_t`

*(No description available)*

## Documentation

/**
    @brief Remove orphaned files and update cache
    @param extract_dir Directory containing generated files
    @param dry_run If true, only report what would be deleted without actually deleting
    @return Number of files removed/would be removed
    */

## Source

**File:** `include/backend\doc\cache.h`

**Lines:** 112-112
