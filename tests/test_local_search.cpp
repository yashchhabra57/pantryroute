#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fixtures.hpp"
#include "pantryroute/insertion.hpp"
#include "pantryroute/local_search.hpp"
#include "pantryroute/savings.hpp"

using namespace pantryroute;
using namespace pantryroute::testing;
using Catch::Approx;

namespace {
// Depot and stops on a line: 0 --10-- 1 --10-- 2 --10-- 3
const std::vector<std::vector<double>> kLine{
    {0, 10, 20, 30},
    {10, 0, 10, 20},
    {20, 10, 0, 10},
    {30, 20, 10, 0},
};

double total_km(const Problem& p, const std::vector<Route>& routes) {
  double km = 0;
  for (const Route& r : routes) km += route_km(p, r);
  return km;
}
}  // namespace

TEST_CASE("2-opt untangles a crossed route") {
  const Problem p = table_problem(kLine, {stop(1), stop(1), stop(1)}, Fleet{1, 10});
  Route r{2, 1, 3};
  REQUIRE(route_km(p, r) == Approx(80));
  REQUIRE(improve_two_opt(p, r) >= 1);
  REQUIRE(route_km(p, r) == Approx(60));
}

TEST_CASE("2-opt refuses reversals that break a time window") {
  // Stop 3 closes at 35, so any accepted reversal must still reach it in time.
  const Problem p = table_problem(kLine, {stop(1), stop(1, 0, 1440), stop(1, 0, 35)}, Fleet{1, 10});
  Route r{3, 1, 2};
  const double before_km = route_km(p, r);
  improve_two_opt(p, r);
  REQUIRE(route_feasible(p, r));
  REQUIRE(route_km(p, r) <= before_km);
}

TEST_CASE("relocate moves a stop onto the route that passes by it") {
  const Problem p = table_problem(kLine, {stop(1), stop(1), stop(1)}, Fleet{2, 10});
  std::vector<Route> routes{{1, 3}, {2}};
  std::size_t moves = 0;
  while (improve_relocate(p, routes)) ++moves;
  REQUIRE(moves >= 1);
  REQUIRE(routes.size() == 1);  // emptied route is removed
  REQUIRE(total_km(p, routes) == Approx(60));
}

TEST_CASE("swap exchanges stops between routes") {
  // Two arms: east 1,2 (near each other) and west 3,4. Start with them mixed up.
  const std::vector<std::vector<double>> arms{
      {0, 10, 10, 10, 10},
      {10, 0, 2, 20, 20},
      {10, 2, 0, 20, 20},
      {10, 20, 20, 0, 2},
      {10, 20, 20, 2, 0},
  };
  const Problem p = table_problem(arms, {stop(1), stop(1), stop(1), stop(1)}, Fleet{2, 2});
  std::vector<Route> routes{{1, 3}, {2, 4}};
  REQUIRE(total_km(p, routes) == Approx(80));
  REQUIRE(improve_swap(p, routes));
  REQUIRE(total_km(p, routes) == Approx(44));
}

TEST_CASE("cross exchanges route tails") {
  const std::vector<std::vector<double>> arms{
      {0, 10, 10, 10, 10},
      {10, 0, 2, 20, 20},
      {10, 2, 0, 20, 20},
      {10, 20, 20, 0, 2},
      {10, 20, 20, 2, 0},
  };
  const Problem p = table_problem(arms, {stop(1), stop(1), stop(1), stop(1)}, Fleet{2, 2});
  std::vector<Route> routes{{1, 4}, {3, 2}};
  REQUIRE(improve_cross(p, routes));
  REQUIRE(total_km(p, routes) == Approx(44));
}

TEST_CASE("local search never worsens and keeps random solutions valid") {
  for (unsigned seed = 1; seed <= 25; ++seed) {
    const Problem p = random_problem(seed, 45, Fleet{4, 45}, 0.4);
    Solution s = savings_construct(p);
    enforce_fleet(p, s);
    const double before_km = solution_stats(p, s).km;
    const MoveCounts moves = local_search(p, s);
    INFO("seed " << seed);
    REQUIRE(validate_solution(p, s).empty());
    REQUIRE(solution_stats(p, s).km <= before_km + 1e-9);
    REQUIRE(moves.rounds >= 1);
  }
}

TEST_CASE("local search reaches a fixed point") {
  const Problem p = random_problem(3, 30, Fleet{3, 40});
  Solution s = savings_construct(p);
  enforce_fleet(p, s);
  local_search(p, s, 1000);
  const MoveCounts again = local_search(p, s, 1000);
  REQUIRE(again.total() == 0);
}
