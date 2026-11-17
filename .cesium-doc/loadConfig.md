---
type: function
name: loadConfig
full_name: loadConfig
start_line: 43
end_line: 43
file: include/backend\doc\config.h
return_type: std::optional<JsonDoc>
---

# loadConfig

*function*

## Signature

```cpp
std::optional<JsonDoc> loadConfig()
```

## Returns

`std::optional<JsonDoc>`

*(No description available)*

## Documentation

/**
@brief Load and validate configuration file

Loads the JSON configuration file and performs basic validation.

@param config_path Path to configuration file
@return Loaded JsonDoc on success, std::nullopt on error
*/

## Source

**File:** `include/backend\doc\config.h`

**Lines:** 43-43
