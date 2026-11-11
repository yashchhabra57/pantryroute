#pragma once

#include <cstddef>

#include "pantryroute/model.hpp"

namespace pantryroute {

struct GeneratorOptions {
  std::size_t stops = 50;
  unsigned seed = 1;
  double narrow_share = 0.3;  // fraction of stops with a 90-minute window
  double radius_deg = 0.15;   // spread of stops around the depot
};

/// Synthetic, reproducible delivery day around a depot in Superior, WI: random
/// demands (1-6 units), 5-minute service, a mix of all-day and narrow windows.
/// Used by the property tests and the benchmark.
Problem generate_problem(const GeneratorOptions& options, Fleet fleet);

}  // namespace pantryroute
