#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "pantryroute/model.hpp"

namespace pantryroute {

struct StopList {
  Stop depot;
  std::vector<Stop> stops;
};

/// Result of reading a stops file. Every problem found is reported (not just the
/// first) so a coordinator can fix the whole spreadsheet in one pass.
struct LoadResult {
  std::optional<StopList> data;
  std::vector<std::string> errors;
  [[nodiscard]] bool ok() const { return data.has_value() && errors.empty(); }
};

/// Reads stops from CSV text.
///
/// Required columns: id, lat, lon. Optional: kind (depot|stop), name, demand,
/// open, close (HH:MM), service (minutes), notes. Column order does not matter.
/// The depot is the row with kind=depot, or the first row when there is no kind column.
/// Stops without a time window inherit the depot's operating hours.
LoadResult load_stops_csv(std::string_view text);

}  // namespace pantryroute
