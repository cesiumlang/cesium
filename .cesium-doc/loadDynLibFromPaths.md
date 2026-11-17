---
type: function
name: loadDynLibFromPaths
full_name: loadDynLibFromPaths
start_line: 106
end_line: 106
file: include/backend\core\dynlib.h
return_type: DynLib
---

# loadDynLibFromPaths

*function*

## Signature

```cpp
DynLib loadDynLibFromPaths()
```

## Returns

`DynLib`

*(No description available)*

## Documentation

/**
@brief Load library by base name from multiple search paths
@param baseName Base name without platform-specific prefix/suffix
@param searchPaths Vector of directories to search (uses defaults if empty)
@return DynLib wrapper or invalid library if not found
*/

## Source

**File:** `include/backend\core\dynlib.h`

**Lines:** 106-106
