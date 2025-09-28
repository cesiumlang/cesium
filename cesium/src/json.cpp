/**
@brief Modern C++ wrapper implementation for yyjson library
*/
#include "json.h"
#include <iostream>

// JsonValue implementations
JsonValue::JsonValue(yyjson_val* val) : val_(val) {}

bool JsonValue::isNull() const { return !val_ || yyjson_is_null(val_); }
bool JsonValue::isString() const { return val_ && yyjson_is_str(val_); }
bool JsonValue::isInt() const { return val_ && yyjson_is_int(val_); }
bool JsonValue::isDouble() const { return val_ && yyjson_is_real(val_); }
bool JsonValue::isArray() const { return val_ && yyjson_is_arr(val_); }
bool JsonValue::isObject() const { return val_ && yyjson_is_obj(val_); }
bool JsonValue::isBool() const { return val_ && yyjson_is_bool(val_); }

std::string JsonValue::asString(const std::string& default_val) const {
  return isString() ? yyjson_get_str(val_) : default_val;
}

int JsonValue::asInt(int default_val) const {
  return isInt() ? yyjson_get_int(val_) : default_val;
}

double JsonValue::asDouble(double default_val) const {
  return isDouble() ? yyjson_get_real(val_) : default_val;
}

bool JsonValue::asBool(bool default_val) const {
  return isBool() ? yyjson_get_bool(val_) : default_val;
}

std::vector<std::string> JsonValue::asStringArray() const {
  std::vector<std::string> result;
  if (isArray()) {
    size_t idx, max;
    yyjson_val* item;
    yyjson_arr_foreach(val_, idx, max, item) {
      if (yyjson_is_str(item)) {
        result.emplace_back(yyjson_get_str(item));
      }
    }
  }
  return result;
}

JsonValue::operator std::string() const { return asString(); }
JsonValue::operator int() const { return asInt(); }
JsonValue::operator double() const { return asDouble(); }
JsonValue::operator bool() const { return asBool(); }
JsonValue::operator std::vector<std::string>() const { return asStringArray(); }

JsonValue JsonValue::operator[](const std::string& key) const {
  return isObject() ? JsonValue(yyjson_obj_get(val_, key.c_str())) : JsonValue(nullptr);
}

JsonValue JsonValue::operator[](size_t index) const {
  return isArray() ? JsonValue(yyjson_arr_get(val_, index)) : JsonValue(nullptr);
}

size_t JsonValue::size() const {
  if (isArray()) {
    return yyjson_arr_size(val_);
  } else if (isObject()) {
    return yyjson_obj_size(val_);
  }
  return 0;
}

// JsonProxy implementations
JsonProxy::JsonProxy(yyjson_mut_doc* doc, yyjson_mut_val* parent, const std::string& key)
  : mut_doc_(doc), parent_(parent), key_(key), index_(0), is_array_access_(false) {}

JsonProxy::JsonProxy(yyjson_mut_doc* doc, yyjson_mut_val* parent, size_t index)
  : mut_doc_(doc), parent_(parent), key_(""), index_(index), is_array_access_(true) {}

JsonProxy::operator std::string() const {
  yyjson_mut_val* val = getValue();
  return val && yyjson_mut_is_str(val) ? yyjson_mut_get_str(val) : "";
}

JsonProxy::operator int() const {
  yyjson_mut_val* val = getValue();
  return val && yyjson_mut_is_int(val) ? yyjson_mut_get_int(val) : 0;
}

JsonProxy::operator double() const {
  yyjson_mut_val* val = getValue();
  return val && yyjson_mut_is_real(val) ? yyjson_mut_get_real(val) : 0.0;
}

JsonProxy::operator bool() const {
  yyjson_mut_val* val = getValue();
  return val && yyjson_mut_is_bool(val) ? yyjson_mut_get_bool(val) : false;
}

JsonProxy::operator std::vector<std::string>() const {
  yyjson_mut_val* val = getValue();
  std::vector<std::string> result;
  if (val && yyjson_mut_is_arr(val)) {
    size_t idx, max;
    yyjson_mut_val* item;
    yyjson_mut_arr_foreach(val, idx, max, item) {
      if (yyjson_mut_is_str(item)) {
        result.emplace_back(yyjson_mut_get_str(item));
      }
    }
  }
  return result;
}

JsonProxy& JsonProxy::operator=(const std::string& value) {
  yyjson_mut_val* str_val = yyjson_mut_str(mut_doc_, value.c_str());
  setValue(str_val);
  return *this;
}

JsonProxy& JsonProxy::operator=(const char* value) {
  return operator=(std::string(value));
}

JsonProxy& JsonProxy::operator=(int value) {
  yyjson_mut_val* int_val = yyjson_mut_int(mut_doc_, value);
  setValue(int_val);
  return *this;
}

JsonProxy& JsonProxy::operator=(double value) {
  yyjson_mut_val* real_val = yyjson_mut_real(mut_doc_, value);
  setValue(real_val);
  return *this;
}

JsonProxy& JsonProxy::operator=(bool value) {
  yyjson_mut_val* bool_val = yyjson_mut_bool(mut_doc_, value);
  setValue(bool_val);
  return *this;
}

JsonProxy& JsonProxy::operator=(const std::vector<std::string>& value) {
  yyjson_mut_val* arr = yyjson_mut_arr(mut_doc_);
  for (const auto& str : value) {
    yyjson_mut_val* str_val = yyjson_mut_str(mut_doc_, str.c_str());
    yyjson_mut_arr_append(arr, str_val);
  }
  setValue(arr);
  return *this;
}

JsonProxy& JsonProxy::operator=(const std::vector<int>& value) {
  yyjson_mut_val* arr = yyjson_mut_arr(mut_doc_);
  for (int i : value) {
    yyjson_mut_val* int_val = yyjson_mut_int(mut_doc_, i);
    yyjson_mut_arr_append(arr, int_val);
  }
  setValue(arr);
  return *this;
}

JsonProxy JsonProxy::operator[](const std::string& key) {
  yyjson_mut_val* val = getValue();
  if (!val || !yyjson_mut_is_obj(val)) {
    val = yyjson_mut_obj(mut_doc_);
    setValue(val);
  }
  return JsonProxy(mut_doc_, val, key);
}

JsonProxy JsonProxy::operator[](size_t index) {
  yyjson_mut_val* val = getValue();
  if (!val || !yyjson_mut_is_arr(val)) {
    val = yyjson_mut_arr(mut_doc_);
    setValue(val);
  }
  return JsonProxy(mut_doc_, val, index);
}

bool JsonProxy::isNull() const {
  yyjson_mut_val* val = getValue();
  return !val || yyjson_mut_is_null(val);
}

yyjson_mut_val* JsonProxy::getValue() const {
  return is_array_access_ ? yyjson_mut_arr_get(parent_, index_) : yyjson_mut_obj_get(parent_, key_.c_str());
}

void JsonProxy::setValue(yyjson_mut_val* value) {
  if (is_array_access_) {
    while (yyjson_mut_arr_size(parent_) <= index_) {
      yyjson_mut_arr_append(parent_, yyjson_mut_null(mut_doc_));
    }
    yyjson_mut_arr_replace(parent_, index_, value);
  } else {
    yyjson_mut_obj_put(parent_, yyjson_mut_str(mut_doc_, key_.c_str()), value);
  }
}

// JsonDoc implementations
JsonDoc::JsonDoc() : doc_(nullptr), mut_doc_(nullptr) {
  mut_doc_ = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val* root = yyjson_mut_obj(mut_doc_);
  yyjson_mut_doc_set_root(mut_doc_, root);
}

JsonDoc::JsonDoc(yyjson_doc* doc) : doc_(doc), mut_doc_(nullptr) {}

JsonDoc::~JsonDoc() {
  if (doc_) yyjson_doc_free(doc_);
  if (mut_doc_) yyjson_mut_doc_free(mut_doc_);
}

JsonDoc::JsonDoc(JsonDoc&& other) noexcept : doc_(other.doc_), mut_doc_(other.mut_doc_) {
  other.doc_ = nullptr;
  other.mut_doc_ = nullptr;
}

JsonDoc& JsonDoc::operator=(JsonDoc&& other) noexcept {
  if (this != &other) {
    if (doc_) yyjson_doc_free(doc_);
    if (mut_doc_) yyjson_mut_doc_free(mut_doc_);
    doc_ = other.doc_;
    mut_doc_ = other.mut_doc_;
    other.doc_ = nullptr;
    other.mut_doc_ = nullptr;
  }
  return *this;
}

std::optional<JsonDoc> JsonDoc::fromFile(const std::string& path) {
  yyjson_read_flag flags = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS | YYJSON_READ_ALLOW_INF_AND_NAN;
  yyjson_read_err err;
  yyjson_doc* doc = yyjson_read_file(path.c_str(), flags, nullptr, &err);
  if (!doc) {
    std::cerr << "JSON parse error at " << err.pos << ": " << err.msg << std::endl;
    return std::nullopt;
  }
  return JsonDoc(doc);
}

JsonProxy JsonDoc::operator[](const std::string& key) {
  ensureMutable();
  yyjson_mut_val* root = yyjson_mut_doc_get_root(mut_doc_);
  return JsonProxy(mut_doc_, root, key);
}

JsonValue JsonDoc::operator[](const std::string& key) const {
  yyjson_val* root = nullptr;
  if (doc_) {
    root = yyjson_doc_get_root(doc_);
  } else if (mut_doc_) {
    root = (yyjson_val*)yyjson_mut_doc_get_root(mut_doc_);
  }
  return yyjson_is_obj(root) ? JsonValue(yyjson_obj_get(root, key.c_str())) : JsonValue(nullptr);
}

bool JsonDoc::writeToFile(const std::string& path, bool pretty) const {
  yyjson_write_flag flags = pretty ? YYJSON_WRITE_PRETTY : 0;
  yyjson_write_err err;
  bool success = false;
  if (mut_doc_) {
    success = yyjson_mut_write_file(path.c_str(), mut_doc_, flags, nullptr, &err);
  } else if (doc_) {
    success = yyjson_write_file(path.c_str(), doc_, flags, nullptr, &err);
  }
  if (!success) {
    std::cerr << "JSON write error: " << err.msg << std::endl;
  }
  return success;
}

bool JsonDoc::isValid() const {
  return doc_ != nullptr || mut_doc_ != nullptr;
}

void JsonDoc::ensureMutable() {
  if (!mut_doc_) {
    if (doc_) {
      mut_doc_ = yyjson_doc_mut_copy(doc_, nullptr);
    } else {
      mut_doc_ = yyjson_mut_doc_new(nullptr);
      yyjson_mut_val* root = yyjson_mut_obj(mut_doc_);
      yyjson_mut_doc_set_root(mut_doc_, root);
    }
  }
}
