---
type: function
namespace: CesiumDocCLI
name: pruneDocs
full_name: CesiumDocCLI::pruneDocs
start_line: 74
end_line: 74
file: include/backend\doc\doc_cli.h
return_type: int
---

# pruneDocs

*function in CesiumDocCLI*

## Signature

```cpp
int pruneDocs()
```

## Returns

`int`

*(No description available)*

## Documentation

/**
    @brief Prune orphaned documentation files
    
    Removes documentation files that are no longer tracked by cache or whose
    source files have been deleted. Supports dry-run mode for preview.
    
    @param argc Command line argument count
    @param argv Command line argument vector
    @return Exit code (0 for success, non-zero for error)
    */

## Source

**File:** `include/backend\doc\doc_cli.h`

**Lines:** 74-74
