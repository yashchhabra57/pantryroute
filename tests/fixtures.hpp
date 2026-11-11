#pragma once

#include <random>
#include <string>
#include <vector>

#include "pantryroute/generator.hpp"
#include "pantryroute/model.hpp"

namespace pantryroute::testing {

/// Builds a problem directly from a symmetric cost table (km == minutes) so tests
/// can be traced by hand. `stops` excludes the depot; costs index 0 is the depot.
inline Problem table_problem(const std::vector<std::vector<double>>& costs, std::vector<Stop> stops,
                             Fleet fleet, int depot_open = 0, int depot_close = 1440) {
  Problem p;
  p.depot.id = "DEPOT";
  p.depot.name = "Depot";
  p.depot.open = depot_open;
  p.depot.close = depot_close;
  p.stops = std::move(stops);
  for (std::size_t i = 0; i < p.stops.size(); ++i) {
    if (p.stops[i].id.empty()) p.stops[i].id = "S" + std::to_string(i + 1);
  }
  p.fleet = fleet;
  p.km = Matrix(costs.size());
  for (std::size_t i = 0; i < costs.size(); ++i) {
    for (std::size_t j = 0; j < costs.size(); ++j) p.km.at(i, j) = costs[i][j];
  }
  p.minutes = p.km;
  return p;
}

inline Stop stop(int demand, int open = 0, int close = 1440, int service = 0) {
  Stop s;
  s.demand = demand;
  s.open = open;
  s.close = close;
  s.service = service;
  return s;
}

/// Random instance (see generate_problem) with the old positional signature.
inline Problem random_problem(unsigned seed, std::size_t n, Fleet fleet, double narrow_share = 0.3) {
  return generate_problem(GeneratorOptions{n, seed, narrow_share, 0.15}, fleet);
}

}  // namespace pantryroute::testing
