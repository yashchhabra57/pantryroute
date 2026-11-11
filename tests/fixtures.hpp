#pragma once

#include <random>
#include <string>
#include <vector>

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

/// Random clustered instance around a depot, with a mix of wide and narrow windows.
inline Problem random_problem(unsigned seed, std::size_t n, Fleet fleet, double narrow_share = 0.3) {
  std::mt19937 rng(seed);
  std::uniform_real_distribution<double> offset(-0.15, 0.15);
  std::uniform_int_distribution<int> demand(1, 6);
  std::uniform_int_distribution<int> window_start(9 * 60, 14 * 60);
  std::uniform_real_distribution<double> coin(0.0, 1.0);

  Stop depot;
  depot.id = "DEPOT";
  depot.pos = {46.72, -92.10};
  depot.open = 8 * 60;
  depot.close = 18 * 60;

  std::vector<Stop> stops;
  for (std::size_t i = 0; i < n; ++i) {
    Stop s;
    s.id = "S" + std::to_string(i + 1);
    s.name = s.id;
    s.pos = {depot.pos.lat + offset(rng), depot.pos.lon + offset(rng) * 1.4};
    s.demand = demand(rng);
    s.service = 5;
    if (coin(rng) < narrow_share) {
      s.open = window_start(rng);
      s.close = s.open + 90;
    } else {
      s.open = depot.open;
      s.close = depot.close;
    }
    stops.push_back(s);
  }
  return build_problem(depot, stops, fleet);
}

}  // namespace pantryroute::testing
