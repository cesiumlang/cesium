---
type: function
namespace: DynamicLanguageLoader
name: getLanguageForFile
full_name: DynamicLanguageLoader::getLanguageForFile
start_line: 63
end_line: 63
file: include/backend\doc\treesitter.h
return_type: std::pair<std::string, const LanguageInfo*>
---

# getLanguageForFile

*function in DynamicLanguageLoader*

## Signature

```cpp
std::pair<std::string, const LanguageInfo*> getLanguageForFile()
```

## Returns

`std::pair<std::string, const LanguageInfo*>`

*(No description available)*

## Documentation

/**
    @brief Find appropriate language parser for a source file
    @param filename Path to source file
    @return Pair of language name and LanguageInfo pointer, or empty if not found
    */

## Source

**File:** `include/backend\doc\treesitter.h`

**Lines:** 63-63
