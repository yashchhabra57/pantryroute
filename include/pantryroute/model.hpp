#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "pantryroute/geo.hpp"

namespace pantryroute {

/// A delivery location (or the depot). Times are minutes after midnight.
struct Stop {
  std::string id;
  std::string name;
  LatLon pos;
  int demand = 0;    // boxes / units that must fit in the vehicle
  int open = 0;      // earliest service start
  int close = 1440;  // latest service start
  int service = 0;   // minutes spent at the stop
  std::string notes;
};

struct Fleet {
  int vehicles = 1;
  int capacity = 1;
  double speed_kmh = 40.0;
  double road_factor = 1.3;
};

/// Node 0 is the depot; node i (1..n) is stops[i - 1].
using Node = std::size_t;
constexpr Node kDepot = 0;

struct Problem {
  Stop depot;
  std::vector<Stop> stops;
  Fleet fleet;
  Matrix km;       // distances between nodes
  Matrix minutes;  // travel times between nodes

  [[nodiscard]] std::size_t node_count() const { return stops.size() + 1; }
  [[nodiscard]] const Stop& node(Node n) const { return n == kDepot ? depot : stops[n - 1]; }
};

/// Validates fleet settings and precomputes the distance/time matrices.
Problem build_problem(Stop depot, std::vector<Stop> stops, Fleet fleet);

}  // namespace pantryroute
