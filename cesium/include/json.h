/**
 * @file json.h
 * @brief Modern C++ wrapper for yyjson library providing safe JSON parsing and manipulation
 */
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <iostream>
#include "yyjson.h"

/**
 * @class JsonValue
 * @brief Read-only wrapper for JSON values from parsed documents
 * 
 * Provides type-safe access to JSON values with automatic type checking
 * and sensible default values for missing or mistyped data.
 */
class JsonValue {
  private:
    yyjson_val* val_;

  public:
    /**
     * @brief Construct JsonValue from yyjson value pointer
     */
    explicit JsonValue(yyjson_val* val);

    // Type checking methods
    bool isNull() const;    ///< Check if value is null
    bool isString() const;  ///< Check if value is string
    bool isInt() const;     ///< Check if value is integer
    bool isDouble() const;  ///< Check if value is double
    bool isArray() const;   ///< Check if value is array
    bool isObject() const;  ///< Check if value is object
    bool isBool() const;    ///< Check if value is boolean

    /**
     * @brief Get value as string with default fallback
     */
    std::string asString(const std::string& default_val = "") const;
    
    /**
     * @brief Get value as integer with default fallback
     */
    int asInt(int default_val = 0) const;
    
    /**
     * @brief Get value as double with default fallback
     */
    double asDouble(double default_val = 0.0) const;
    
    /**
     * @brief Get value as boolean with default fallback
     */
    bool asBool(bool default_val = false) const;
    
    /**
     * @brief Convert JSON array to vector of strings
     */
    std::vector<std::string> asStringArray() const;

    // Explicit conversions for convenience
    explicit operator std::string() const;
    explicit operator int() const;
    explicit operator double() const;
    explicit operator bool() const;
    explicit operator std::vector<std::string>() const;

    // Read-only access to nested values
    JsonValue operator[](const std::string& key) const;
    JsonValue operator[](size_t index) const;

    // Iteration support - overloaded for object vs array callbacks
    void forEachObject(std::function<void(const std::string&, JsonValue)> callback) const {
      if (isObject()) {
        size_t idx, max;
        yyjson_val* key, *val;
        yyjson_obj_foreach(val_, idx, max, key, val) {
          callback(yyjson_get_str(key), JsonValue(val));
        }
      }
    }
    
    void forEachArray(std::function<void(size_t, JsonValue)> callback) const {
      if (isArray()) {
        size_t idx, max;
        yyjson_val* item;
        yyjson_arr_foreach(val_, idx, max, item) {
          callback(idx, JsonValue(item));
        }
      }
    }
    
    // Legacy template forEach - automatically detects type and calls appropriate method
    template<typename Func> void forEach(Func callback) const {
      if (isObject()) {
        forEachObject([callback](const std::string& key, JsonValue value) {
          callback(key, value);
        });
      } else if (isArray()) {
        forEachArray([callback](size_t idx, JsonValue value) {
          callback(idx, value);
        });
      }
    }

    size_t size() const;
};

class JsonProxy {
  private:
    yyjson_mut_doc* mut_doc_;
    yyjson_mut_val* parent_;
    std::string key_;
    size_t index_;
    bool is_array_access_;

  public:
    JsonProxy(yyjson_mut_doc* doc, yyjson_mut_val* parent, const std::string& key);
    JsonProxy(yyjson_mut_doc* doc, yyjson_mut_val* parent, size_t index);

    // Reading (implicit conversion)
    operator std::string() const;
    operator int() const;
    operator double() const;
    operator bool() const;
    operator std::vector<std::string>() const;

    // Writing (assignment operators)
    JsonProxy& operator=(const std::string& value);
    JsonProxy& operator=(const char* value);
    JsonProxy& operator=(int value);
    JsonProxy& operator=(double value);
    JsonProxy& operator=(bool value);
    JsonProxy& operator=(const std::vector<std::string>& value);
    JsonProxy& operator=(const std::vector<int>& value);

    // Chaining for nested access
    JsonProxy operator[](const std::string& key);
    JsonProxy operator[](size_t index);

    // Type checking and utility methods
    bool isNull() const;

    template<typename Func>
    void forEach(Func callback) const {
      yyjson_mut_val* val = getValue();
      if (val && yyjson_mut_is_obj(val)) {
        size_t idx, max;
        yyjson_mut_val* key, *value;
        yyjson_mut_obj_foreach(val, idx, max, key, value) {
          callback(yyjson_mut_get_str(key), JsonProxy(mut_doc_, val, yyjson_mut_get_str(key)));
        }
      }
    }

  private:
    yyjson_mut_val* getValue() const;
    void setValue(yyjson_mut_val* value);
};

/**
 * @class JsonDoc
 * @brief Main JSON document class supporting both reading and writing
 * 
 * RAII wrapper around yyjson documents that provides safe parsing,
 * modification, and serialization of JSON data.
 */
class JsonDoc {
  private:
    yyjson_doc* doc_;      ///< Read-only document handle
    yyjson_mut_doc* mut_doc_; ///< Mutable document handle

  public:
    /**
     * @brief Create empty JSON document
     */
    JsonDoc();
    
    /**
     * @brief Construct from existing yyjson document
     */
    explicit JsonDoc(yyjson_doc* doc);
    
    /**
     * @brief Destructor - automatically frees document memory
     */
    ~JsonDoc();

    // Move semantics for safe transfer
    JsonDoc(JsonDoc&& other) noexcept;
    JsonDoc& operator=(JsonDoc&& other) noexcept;

    // Disable copy to prevent double-free
    JsonDoc(const JsonDoc&) = delete;
    JsonDoc& operator=(const JsonDoc&) = delete;

    /**
     * @brief Load JSON document from file
     * @param path Path to JSON file
     * @return JsonDoc wrapped in optional, or nullopt on parse error
     */
    static std::optional<JsonDoc> fromFile(const std::string& path);

    /**
     * @brief Access/modify JSON value by key (mutable)
     */
    JsonProxy operator[](const std::string& key);
    
    /**
     * @brief Access JSON value by key (read-only)
     */
    JsonValue operator[](const std::string& key) const;

    /**
     * @brief Write JSON document to file
     * @param path Output file path
     * @param pretty Whether to format with indentation
     * @return True if write succeeded, false on error
     */
    bool writeToFile(const std::string& path, bool pretty = true) const;
    
    /**
     * @brief Check if document was parsed successfully
     */
    bool isValid() const;

  private:
    void ensureMutable();
};
