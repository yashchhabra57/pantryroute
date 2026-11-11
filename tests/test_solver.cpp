#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "fixtures.hpp"
#include "pantryroute/solver.hpp"

using namespace pantryroute;
using namespace pantryroute::testing;
using Catch::Approx;

TEST_CASE("solves a hand-checkable two-cluster instance optimally") {
  const std::vector<std::vector<double>> arms{
      {0, 10, 10, 10, 10},
      {10, 0, 2, 20, 20},
      {10, 2, 0, 20, 20},
      {10, 20, 20, 0, 2},
      {10, 20, 20, 2, 0},
  };
  const Problem p = table_problem(arms, {stop(1), stop(1), stop(1), stop(1)}, Fleet{2, 2});
  const SolveReport r = solve(p);
  REQUIRE(validate_solution(p, r.solution).empty());
  REQUIRE(r.final.km == Approx(44));
  REQUIRE(r.final.vehicles_used == 2);
  REQUIRE(r.final.unserved == 0);
}

TEST_CASE("property: every random instance yields a valid, no-worse plan") {
  const auto seed = GENERATE(Catch::Generators::range(1u, 41u));
  const auto vehicles = GENERATE(1, 3, 6);
  const Problem p = random_problem(seed, 35, Fleet{vehicles, 30}, 0.35);
  const SolveReport r = solve(p);
  INFO("seed " << seed << " vehicles " << vehicles);
  REQUIRE(validate_solution(p, r.solution).empty());
  REQUIRE(r.final.served + r.final.unserved == p.stops.size());
  // Local search may serve extra stops (more km), but never serves fewer.
  REQUIRE(r.final.unserved <= r.construction.unserved);
  if (r.final.unserved == r.construction.unserved) {
    REQUIRE(r.final.km <= r.construction.km + 1e-9);
  }
}

TEST_CASE("local search improves on construction for medium instances") {
  double construction = 0;
  double final_km = 0;
  for (unsigned seed = 100; seed < 110; ++seed) {
    const Problem p = random_problem(seed, 80, Fleet{8, 40}, 0.3);
    const SolveReport r = solve(p);
    construction += r.construction.km;
    final_km += r.final.km;
  }
  // Across instances, local search should shave a visible amount off savings.
  REQUIRE(final_km < construction * 0.99);
}

TEST_CASE("solve is deterministic") {
  const Problem p = random_problem(42, 60, Fleet{5, 35});
  const SolveReport a = solve(p);
  const SolveReport b = solve(p);
  REQUIRE(a.solution.routes == b.solution.routes);
  REQUIRE(a.solution.unserved == b.solution.unserved);
}

TEST_CASE("construction-only mode skips local search") {
  const Problem p = random_problem(5, 30, Fleet{4, 35});
  const SolveReport r = solve(p, SolveOptions{false, 0});
  REQUIRE(r.moves.total() == 0);
  REQUIRE(validate_solution(p, r.solution).empty());
}
