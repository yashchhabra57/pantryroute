#include "pantryroute/generator.hpp"

#include <random>
#include <string>

namespace pantryroute {

Problem generate_problem(const GeneratorOptions& options, Fleet fleet) {
  // std::mt19937 is fully specified by the standard, but the distributions are not,
  // so results can differ slightly between standard libraries (fine for tests that
  // only check invariants).
  std::mt19937 rng(options.seed);
  std::uniform_real_distribution<double> offset(-options.radius_deg, options.radius_deg);
  std::uniform_int_distribution<int> demand(1, 6);
  std::uniform_int_distribution<int> window_start(9 * 60, 14 * 60);
  std::uniform_real_distribution<double> coin(0.0, 1.0);

  Stop depot;
  depot.id = "DEPOT";
  depot.name = "Depot";
  depot.pos = {46.72, -92.10};
  depot.open = 8 * 60;
  depot.close = 18 * 60;

  std::vector<Stop> stops;
  stops.reserve(options.stops);
  for (std::size_t i = 0; i < options.stops; ++i) {
    Stop s;
    s.id = "S" + std::to_string(i + 1);
    s.name = s.id;
    // Longitude degrees are shorter this far north; stretch so the cloud is round.
    s.pos = {depot.pos.lat + offset(rng), depot.pos.lon + offset(rng) * 1.4};
    s.demand = demand(rng);
    s.service = 5;
    if (coin(rng) < options.narrow_share) {
      s.open = window_start(rng);
      s.close = s.open + 90;
    } else {
      s.open = depot.open;
      s.close = depot.close;
    }
    stops.push_back(std::move(s));
  }
  return build_problem(depot, std::move(stops), fleet);
}

}  // namespace pantryroute
