---
type: function
name: loadDynLibFromConfig
full_name: loadDynLibFromConfig
start_line: 141
end_line: 141
file: include/backend\core\dynlib.h
return_type: DynLib
---

# loadDynLibFromConfig

*function*

## Signature

```cpp
DynLib loadDynLibFromConfig()
```

## Returns

`DynLib`

*(No description available)*

## Documentation

/**
@brief Load library using config-based priority search strategy
@param libraryPath Library path from config (can be absolute, relative, or basename)
@param configFilePath Path to the config file (for relative path resolution)
@return DynLib wrapper or invalid library if not found
*/

## Source

**File:** `include/backend\core\dynlib.h`

**Lines:** 141-141
