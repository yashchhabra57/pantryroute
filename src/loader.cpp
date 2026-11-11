#include "pantryroute/loader.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <set>

#include "pantryroute/clock.hpp"
#include "pantryroute/csv.hpp"

namespace pantryroute {

namespace {

std::string trim(const std::string& s) {
  const auto first = s.find_first_not_of(" \t");
  if (first == std::string::npos) return "";
  const auto last = s.find_last_not_of(" \t");
  return s.substr(first, last - first + 1);
}

std::string trim_lower(const std::string& raw) {
  std::string s = trim(raw);
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

template <typename T>
std::optional<T> parse_number(const std::string& raw) {
  const std::string s = trim(raw);
  if (s.empty()) return std::nullopt;
  T value{};
  const char* end = s.data() + s.size();
  const auto [ptr, ec] = std::from_chars(s.data(), end, value);
  if (ec != std::errc{} || ptr != end) return std::nullopt;
  return value;
}

struct Columns {
  std::map<std::string, std::size_t> index;
  [[nodiscard]] bool has(const std::string& name) const { return index.contains(name); }
  [[nodiscard]] std::string get(const CsvRow& row, const std::string& name) const {
    const auto it = index.find(name);
    if (it == index.end() || it->second >= row.fields.size()) return "";
    return trim(row.fields[it->second]);
  }
};

struct ParsedRow {
  Stop stop;
  bool is_depot = false;
  bool has_open = false;
  bool has_close = false;
};

}  // namespace

LoadResult load_stops_csv(std::string_view text) {
  LoadResult result;
  std::vector<CsvRow> rows;
  try {
    rows = parse_csv(text);
  } catch (const CsvError& e) {
    result.errors.emplace_back(e.what());
    return result;
  }
  if (rows.empty()) {
    result.errors.emplace_back("file is empty");
    return result;
  }

  Columns cols;
  for (std::size_t i = 0; i < rows[0].fields.size(); ++i) {
    const std::string name = trim_lower(rows[0].fields[i]);
    if (!name.empty() && !cols.index.emplace(name, i).second) {
      result.errors.push_back("header: duplicate column '" + name + "'");
    }
  }
  for (const char* required : {"id", "lat", "lon"}) {
    if (!cols.has(required)) {
      result.errors.push_back(std::string("header: missing required column '") + required + "'");
    }
  }
  if (!result.errors.empty()) return result;
  if (rows.size() < 2) {
    result.errors.emplace_back("no data rows");
    return result;
  }

  const bool has_kind = cols.has("kind");
  std::vector<ParsedRow> parsed;
  std::set<std::string> seen_ids;

  for (std::size_t r = 1; r < rows.size(); ++r) {
    const CsvRow& row = rows[r];
    const std::string where = "line " + std::to_string(row.line) + ": ";
    auto fail = [&](const std::string& msg) { result.errors.push_back(where + msg); };
    const std::size_t errors_before = result.errors.size();

    ParsedRow pr;
    Stop& s = pr.stop;
    s.id = cols.get(row, "id");
    s.name = cols.get(row, "name");
    s.notes = cols.get(row, "notes");
    if (s.name.empty()) s.name = s.id;

    if (s.id.empty()) {
      fail("id is empty");
    } else if (!seen_ids.insert(s.id).second) {
      fail("duplicate id '" + s.id + "'");
    }

    if (has_kind) {
      const std::string kind = trim_lower(cols.get(row, "kind"));
      if (kind == "depot") {
        pr.is_depot = true;
      } else if (kind != "stop" && !kind.empty()) {
        fail("kind must be 'depot' or 'stop', got '" + kind + "'");
      }
    } else {
      pr.is_depot = (r == 1);
    }

    const auto lat = parse_number<double>(cols.get(row, "lat"));
    const auto lon = parse_number<double>(cols.get(row, "lon"));
    if (!lat || *lat < -90.0 || *lat > 90.0) fail("lat must be a number between -90 and 90");
    if (!lon || *lon < -180.0 || *lon > 180.0) fail("lon must be a number between -180 and 180");
    if (lat && lon) s.pos = {*lat, *lon};

    if (const std::string raw = cols.get(row, "demand"); !raw.empty()) {
      const auto demand = parse_number<int>(raw);
      if (!demand || *demand < 0) {
        fail("demand must be a whole number >= 0");
      } else {
        s.demand = *demand;
      }
    }
    if (const std::string raw = cols.get(row, "service"); !raw.empty()) {
      const auto service = parse_number<int>(raw);
      if (!service || *service < 0) {
        fail("service must be whole minutes >= 0");
      } else {
        s.service = *service;
      }
    }
    if (const std::string raw = cols.get(row, "open"); !raw.empty()) {
      const auto t = parse_clock(raw);
      if (!t) {
        fail("open must be HH:MM, got '" + raw + "'");
      } else {
        s.open = *t;
        pr.has_open = true;
      }
    }
    if (const std::string raw = cols.get(row, "close"); !raw.empty()) {
      const auto t = parse_clock(raw);
      if (!t) {
        fail("close must be HH:MM, got '" + raw + "'");
      } else {
        s.close = *t;
        pr.has_close = true;
      }
    }
    if (pr.has_open && pr.has_close && s.open > s.close) fail("open is after close");

    if (result.errors.size() == errors_before) parsed.push_back(std::move(pr));
  }

  const auto depots =
      std::count_if(parsed.begin(), parsed.end(), [](const ParsedRow& p) { return p.is_depot; });
  if (result.errors.empty() && depots != 1) {
    result.errors.push_back("expected exactly one depot row, found " + std::to_string(depots));
  }
  if (!result.errors.empty()) return result;

  StopList list;
  for (auto& p : parsed) {
    if (p.is_depot) {
      list.depot = std::move(p.stop);
      list.depot.demand = 0;
    }
  }
  for (auto& p : parsed) {
    if (p.is_depot) continue;
    if (!p.has_open) p.stop.open = list.depot.open;
    if (!p.has_close) p.stop.close = list.depot.close;
    if (p.stop.open > p.stop.close) {
      result.errors.push_back("stop '" + p.stop.id + "': window does not overlap depot hours");
    }
    list.stops.push_back(std::move(p.stop));
  }
  if (list.stops.empty()) result.errors.emplace_back("no delivery stops (only a depot)");
  if (result.errors.empty()) result.data = std::move(list);
  return result;
}

}  // namespace pantryroute
