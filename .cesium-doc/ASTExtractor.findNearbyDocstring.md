---
type: function
namespace: ASTExtractor
name: findNearbyDocstring
full_name: ASTExtractor::findNearbyDocstring
start_line: 174
end_line: 174
file: include/backend\doc\cpp\ast_extractor.h
return_type: std::optional<std::string>
---

# findNearbyDocstring

*function in ASTExtractor*

## Signature

```cpp
std::optional<std::string> findNearbyDocstring()
```

## Returns

`std::optional<std::string>`

*(No description available)*

## Documentation

/**
    @brief Find docstring near an AST node (above or below)
    @param node AST node to search near
    @param content Original source code content
    @return Optional docstring if found within proximity threshold
    */

## Source

**File:** `include/backend\doc\cpp\ast_extractor.h`

**Lines:** 174-174
