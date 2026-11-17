---
type: function
namespace: DynLib
name: getFunc
full_name: DynLib::getFunc
start_line: 72
end_line: 74
file: include/backend\core\dynlib.h
return_type: FuncType
---

# getFunc

*function in DynLib*

## Signature

```cpp
FuncType getFunc(const)
```

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `` | `const` | *(No description available)* |

## Returns

`FuncType`

*(No description available)*

## Documentation

/**
    @brief Get function pointer from library
    @tparam FuncType Function pointer type to cast to
    @param name Symbol name to resolve
    @return Function pointer or nullptr if not found
    */

## Source

**File:** `include/backend\core\dynlib.h`

**Lines:** 72-74
