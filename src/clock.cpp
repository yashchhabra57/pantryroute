#include "pantryroute/clock.hpp"

#include <cmath>
#include <string>

namespace pantryroute {

std::optional<int> parse_clock(std::string_view text) {
  const auto colon = text.find(':');
  if (colon == std::string_view::npos || colon == 0 || colon > 2 || text.size() - colon != 3) {
    return std::nullopt;
  }
  int hours = 0;
  for (std::size_t i = 0; i < colon; ++i) {
    const char c = text[i];
    if (c < '0' || c > '9') return std::nullopt;
    hours = hours * 10 + (c - '0');
  }
  int mins = 0;
  for (std::size_t i = colon + 1; i < text.size(); ++i) {
    const char c = text[i];
    if (c < '0' || c > '9') return std::nullopt;
    mins = mins * 10 + (c - '0');
  }
  if (mins > 59 || hours > 24 || (hours == 24 && mins != 0)) return std::nullopt;
  return hours * 60 + mins;
}

std::string format_clock(double minutes) {
  const long total = std::lround(minutes);
  // Built with std::string instead of snprintf into a fixed buffer: no truncation
  // is possible, and GCC 15's -Wformat-truncation has nothing to flag.
  const auto two_digits = [](long v) {
    std::string s = std::to_string(v);
    return s.size() < 2 ? "0" + s : s;
  };
  return two_digits(total / 60) + ":" + two_digits(total % 60);
}

}  // namespace pantryroute
