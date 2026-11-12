#include "pantryroute/model.hpp"

#include <stdexcept>

namespace pantryroute {

Problem build_problem(Stop depot, std::vector<Stop> stops, const Fleet& fleet) {
  if (fleet.vehicles < 1) throw std::invalid_argument("need at least one vehicle");
  if (fleet.capacity < 1) throw std::invalid_argument("vehicle capacity must be positive");

  std::vector<LatLon> points;
  points.reserve(stops.size() + 1);
  points.push_back(depot.pos);
  for (const auto& s : stops) points.push_back(s.pos);

  Problem p;
  p.km = distance_matrix_km(points, fleet.road_factor);
  p.minutes = travel_minutes(p.km, fleet.speed_kmh);
  p.depot = std::move(depot);
  p.stops = std::move(stops);
  p.fleet = fleet;
  return p;
}

}  // namespace pantryroute
