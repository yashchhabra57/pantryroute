#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace pantryroute {

/// Minimal streaming JSON writer (no dependencies). Handles commas and string
/// escaping; callers are responsible for balanced begin/end calls.
class JsonWriter {
 public:
  explicit JsonWriter(bool pretty = true) : pretty_(pretty) {}

  JsonWriter& begin_object();
  JsonWriter& end_object();
  JsonWriter& begin_array();
  JsonWriter& end_array();
  JsonWriter& key(std::string_view k);
  JsonWriter& value(std::string_view v);
  JsonWriter& value(const char* v) { return value(std::string_view(v)); }
  JsonWriter& value(double v);
  JsonWriter& value(int v);
  JsonWriter& value(std::size_t v);
  JsonWriter& value(bool v);
  JsonWriter& null();

  [[nodiscard]] const std::string& str() const { return out_; }

  static std::string escape(std::string_view s);

 private:
  void before_value();
  void newline();

  std::string out_;
  bool pretty_;
  // One entry per open container: true once it holds at least one element.
  std::vector<bool> has_items_;
  bool after_key_ = false;
};

}  // namespace pantryroute
