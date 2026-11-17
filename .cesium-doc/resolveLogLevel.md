---
type: function
name: resolveLogLevel
full_name: resolveLogLevel
start_line: 107
end_line: 107
file: include/backend\core\logging.h
return_type: MessageType
---

# resolveLogLevel

*function*

## Signature

```cpp
MessageType resolveLogLevel()
```

## Returns

`MessageType`

*(No description available)*

## Documentation

/**
  @brief Resolve string log level name to MessageType (case-insensitive)
  @param level_name Log level name (e.g., "info", "WARNING", "Debug")
  @return Corresponding MessageType, or MessageType::Default if unrecognized
  */

## Source

**File:** `include/backend\core\logging.h`

**Lines:** 107-107
