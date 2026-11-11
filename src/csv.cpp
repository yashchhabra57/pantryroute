#include "pantryroute/csv.hpp"

namespace pantryroute {

std::vector<CsvRow> parse_csv(std::string_view text) {
  if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);

  std::vector<CsvRow> rows;
  CsvRow row;
  std::string field;
  bool in_quotes = false;
  bool field_was_quoted = false;
  bool row_has_content = false;
  std::size_t line = 1;
  row.line = 1;

  auto end_field = [&] {
    row.fields.push_back(std::move(field));
    field.clear();
    field_was_quoted = false;
  };
  auto end_row = [&] {
    if (row_has_content) {
      end_field();
      rows.push_back(std::move(row));
    }
    row = CsvRow{};
    field.clear();
    field_was_quoted = false;
    row_has_content = false;
  };

  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (in_quotes) {
      if (c == '"') {
        if (i + 1 < text.size() && text[i + 1] == '"') {
          field.push_back('"');
          ++i;
        } else {
          in_quotes = false;
        }
      } else {
        if (c == '\n') ++line;
        field.push_back(c);
      }
      continue;
    }
    switch (c) {
      case '"':
        if (!field.empty() || field_was_quoted) {
          throw CsvError(line, "unexpected quote inside an unquoted field");
        }
        in_quotes = true;
        field_was_quoted = true;
        row_has_content = true;
        break;
      case ',':
        row_has_content = true;
        end_field();
        break;
      case '\r':
        break;  // tolerate CRLF
      case '\n':
        end_row();
        ++line;
        row.line = line;
        break;
      default:
        if (field_was_quoted) {
          throw CsvError(line, "unexpected character after closing quote");
        }
        field.push_back(c);
        row_has_content = true;
    }
  }
  if (in_quotes) throw CsvError(row.line, "unterminated quoted field");
  end_row();
  return rows;
}

}  // namespace pantryroute
