#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pantryroute {

struct CsvRow {
  std::size_t line = 0;  // 1-based line where the record starts
  std::vector<std::string> fields;
};

class CsvError : public std::runtime_error {
 public:
  CsvError(std::size_t line, const std::string& message)
      : std::runtime_error("line " + std::to_string(line) + ": " + message), line_(line) {}
  [[nodiscard]] std::size_t line() const { return line_; }

 private:
  std::size_t line_;
};

/// RFC 4180-style CSV parser: comma separated, double-quote escaping ("" inside quotes),
/// quoted fields may contain commas and newlines. Accepts LF or CRLF line endings, skips
/// blank lines and a leading UTF-8 BOM (Excel exports add one).
std::vector<CsvRow> parse_csv(std::string_view text);

}  // namespace pantryroute
