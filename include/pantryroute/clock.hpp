#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace pantryroute {

/// Parses a 24-hour "HH:MM" string into minutes after midnight.
/// Accepts "9:05" and "09:05"; "24:00" is allowed as end-of-day.
std::optional<int> parse_clock(std::string_view text);

/// Formats minutes after midnight as "HH:MM" (rounded to the nearest minute).
std::string format_clock(double minutes);

}  // namespace pantryroute
