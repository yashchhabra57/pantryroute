#include "pantryroute/json.hpp"

#include <cmath>
#include <cstdio>

namespace pantryroute {

std::string JsonWriter::escape(std::string_view s) {
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('"');
  for (const char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
          out += buf;
        } else {
          out.push_back(c);
        }
    }
  }
  out.push_back('"');
  return out;
}

void JsonWriter::newline() {
  if (!pretty_) return;
  out_.push_back('\n');
  out_.append(has_items_.size() * 2, ' ');
}

void JsonWriter::before_value() {
  if (after_key_) {
    after_key_ = false;
    return;
  }
  if (!has_items_.empty()) {
    if (has_items_.back()) out_.push_back(',');
    has_items_.back() = true;
    newline();
  }
}

JsonWriter& JsonWriter::begin_object() {
  before_value();
  out_.push_back('{');
  has_items_.push_back(false);
  return *this;
}

JsonWriter& JsonWriter::end_object() {
  const bool had = has_items_.back();
  has_items_.pop_back();
  if (had) newline();
  out_.push_back('}');
  return *this;
}

JsonWriter& JsonWriter::begin_array() {
  before_value();
  out_.push_back('[');
  has_items_.push_back(false);
  return *this;
}

JsonWriter& JsonWriter::end_array() {
  const bool had = has_items_.back();
  has_items_.pop_back();
  if (had) newline();
  out_.push_back(']');
  return *this;
}

JsonWriter& JsonWriter::key(std::string_view k) {
  before_value();
  out_ += escape(k);
  out_ += pretty_ ? ": " : ":";
  after_key_ = true;
  return *this;
}

JsonWriter& JsonWriter::value(std::string_view v) {
  before_value();
  out_ += escape(v);
  return *this;
}

JsonWriter& JsonWriter::value(double v) {
  before_value();
  if (!std::isfinite(v)) {
    out_ += "null";  // JSON has no NaN/Infinity
    return *this;
  }
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.3f", v);
  // Trim trailing zeros ("12.500" -> "12.5", "3.000" -> "3").
  std::string s(buf);
  if (s.find('.') != std::string::npos) {
    while (s.back() == '0') s.pop_back();
    if (s.back() == '.') s.pop_back();
  }
  if (s == "-0") s = "0";
  out_ += s;
  return *this;
}

JsonWriter& JsonWriter::value(int v) {
  before_value();
  out_ += std::to_string(v);
  return *this;
}

JsonWriter& JsonWriter::value(std::size_t v) {
  before_value();
  out_ += std::to_string(v);
  return *this;
}

JsonWriter& JsonWriter::value(bool v) {
  before_value();
  out_ += v ? "true" : "false";
  return *this;
}

JsonWriter& JsonWriter::null() {
  before_value();
  out_ += "null";
  return *this;
}

}  // namespace pantryroute
