#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fixtures.hpp"
#include "pantryroute/insertion.hpp"
#include "pantryroute/savings.hpp"

using namespace pantryroute;
using namespace pantryroute::testing;
using Catch::Approx;

namespace {
// Line: depot 0 --10-- 1 --10-- 2 --10-- 3
const std::vector<std::vector<double>> kLine{
    {0, 10, 20, 30},
    {10, 0, 10, 20},
    {20, 10, 0, 10},
    {30, 20, 10, 0},
};
}  // namespace

TEST_CASE("cheapest insertion finds the free spot on a straight line") {
  const Problem p = table_problem(kLine, {stop(1), stop(1), stop(1)}, Fleet{1, 10});
  const std::vector<Route> routes{{1, 3}};
  const auto at = cheapest_insertion(p, routes, 2);
  REQUIRE(at.has_value());
  REQUIRE(at->position == 1);
  REQUIRE(at->added_km == Approx(0));
}

TEST_CASE("cheapest insertion skips infeasible positions") {
  // Stop 1 closes at 10: inserting 2 before it would make it late.
  const Problem p = table_problem(kLine, {stop(1, 0, 10), stop(1), stop(1)}, Fleet{1, 10});
  const auto at = cheapest_insertion(p, {{1}}, 2);
  REQUIRE(at.has_value());
  REQUIRE(at->position == 1);
  REQUIRE_FALSE(cheapest_insertion(table_problem(kLine, {stop(9), stop(9), stop(1)}, Fleet{1, 10}), {{1}}, 2));
}

TEST_CASE("insert_node opens a new route only when a vehicle is free") {
  const Problem p = table_problem(kLine, {stop(6), stop(6), stop(1)}, Fleet{2, 10});
  std::vector<Route> routes{{1}};
  REQUIRE(insert_node(p, routes, 2, true));
  REQUIRE(routes.size() == 2);
  // Both vehicles busy, 3 still fits on the second one.
  REQUIRE(insert_node(p, routes, 3, true));
  REQUIRE(routes.size() == 2);

  const Problem tight = table_problem(kLine, {stop(6), stop(6), stop(1)}, Fleet{1, 10});
  std::vector<Route> one{{1}};
  REQUIRE_FALSE(insert_node(tight, one, 2, true));
}

TEST_CASE("enforce_fleet dissolves extra routes and keeps the solution valid") {
  for (unsigned seed = 1; seed <= 15; ++seed) {
    const Problem p = random_problem(seed, 40, Fleet{3, 40});
    Solution s = savings_construct(p);
    enforce_fleet(p, s);
    INFO("seed " << seed);
    REQUIRE(s.routes.size() <= 3);
    REQUIRE(validate_solution(p, s).empty());
  }
}

TEST_CASE("reinsert_unserved fills spare vehicles") {
  const Problem p = table_problem(kLine, {stop(6), stop(6), stop(6)}, Fleet{3, 10});
  Solution s{{{1}}, {2, 3}};
  REQUIRE(reinsert_unserved(p, s) == 2);
  REQUIRE(s.unserved.empty());
  REQUIRE(validate_solution(p, s).empty());
}
