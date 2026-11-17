---
type: function
name: findDefaultConfigFile
full_name: findDefaultConfigFile
start_line: 21
end_line: 21
file: include/backend\doc\config.h
return_type: std::string
---

# findDefaultConfigFile

*function*

## Signature

```cpp
std::string findDefaultConfigFile()
```

## Returns

`std::string`

*(No description available)*

## Documentation

/**
@brief Find the default configuration file in current directory

Searches for cesium-doc-config.jsonc first (preferred), then cesium-doc-config.json.
Issues a warning if both exist.

@return Path to default config file, or empty string if none found
*/

## Source

**File:** `include/backend\doc\config.h`

**Lines:** 21-21
