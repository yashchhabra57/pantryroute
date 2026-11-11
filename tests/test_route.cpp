#include <algorithm>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fixtures.hpp"
#include "pantryroute/route.hpp"

using namespace pantryroute;
using namespace pantryroute::testing;
using Catch::Approx;

namespace {
// Depot and three stops on a line: 0 --10-- 1 --10-- 2 --10-- 3
const std::vector<std::vector<double>> kLine{
    {0, 10, 20, 30},
    {10, 0, 10, 20},
    {20, 10, 0, 10},
    {30, 20, 10, 0},
};
}  // namespace

TEST_CASE("evaluates distance, schedule and load of a feasible route") {
  const Problem p = table_problem(kLine, {stop(2, 0, 1440, 5), stop(3), stop(1)}, Fleet{1, 10}, 480, 1080);
  const RouteEval ev = evaluate_route(p, {1, 2, 3}, true);
  REQUIRE(ev.feasible());
  REQUIRE(ev.km == Approx(60));
  REQUIRE(ev.load == 6);
  REQUIRE(ev.visits.size() == 3);
  REQUIRE(ev.visits[0].arrival == Approx(490));
  REQUIRE(ev.visits[0].departure == Approx(495));  // 5 min service
  REQUIRE(ev.visits[1].arrival == Approx(505));
  REQUIRE(ev.visits[2].load_after == 0);
  REQUIRE(ev.return_depot == Approx(545));
  REQUIRE(ev.duration() == Approx(65));
  REQUIRE(route_feasible(p, {1, 2, 3}));
  REQUIRE(route_km(p, {1, 2, 3}) == Approx(60));
}

TEST_CASE("waits for a window to open") {
  const Problem p = table_problem(kLine, {stop(1, 600, 700), stop(1), stop(1)}, Fleet{1, 10}, 480, 1080);
  const RouteEval ev = evaluate_route(p, {1}, true);
  REQUIRE(ev.feasible());
  REQUIRE(ev.visits[0].arrival == Approx(490));
  REQUIRE(ev.visits[0].start == Approx(600));
  REQUIRE(ev.wait == Approx(110));
}

TEST_CASE("detects capacity, time-window and shift-end violations") {
  SECTION("capacity") {
    const Problem p = table_problem(kLine, {stop(6), stop(5), stop(1)}, Fleet{1, 10});
    REQUIRE(evaluate_route(p, {1, 2}).violation == Violation::Capacity);
    REQUIRE_FALSE(route_feasible(p, {1, 2}));
  }
  SECTION("late arrival names the stop") {
    const Problem p = table_problem(kLine, {stop(1, 0, 1440, 5), stop(1, 0, 1440, 5), stop(1, 0, 35)}, Fleet{1, 10});
    const RouteEval ev = evaluate_route(p, {1, 2, 3});
    REQUIRE(ev.violation == Violation::TimeWindow);
    REQUIRE(ev.violating_node == 3);
    REQUIRE_FALSE(route_feasible(p, {1, 2, 3}));
    // Going straight there is fine.
    REQUIRE(route_feasible(p, {3}));
  }
  SECTION("return after the depot closes") {
    const Problem p = table_problem(kLine, {stop(1), stop(1), stop(1)}, Fleet{1, 10}, 0, 59);
    REQUIRE(evaluate_route(p, {1, 2, 3}).violation == Violation::ShiftEnd);
    REQUIRE_FALSE(route_feasible(p, {1, 2, 3}));
  }
}

TEST_CASE("route_feasible agrees with evaluate_route on random routes") {
  const Problem p = random_problem(7, 30, Fleet{4, 25});
  std::mt19937 rng(99);
  for (int trial = 0; trial < 500; ++trial) {
    Route r;
    for (Node n = 1; n < p.node_count(); ++n) {
      if (rng() % 4 == 0) r.push_back(n);
    }
    std::shuffle(r.begin(), r.end(), rng);
    REQUIRE(route_feasible(p, r) == evaluate_route(p, r).feasible());
  }
}

TEST_CASE("validate_solution catches duplicates, missing stops and fleet overuse") {
  const Problem p = table_problem(kLine, {stop(1), stop(1), stop(1)}, Fleet{1, 10});
  REQUIRE(validate_solution(p, Solution{{{1, 2, 3}}, {}}).empty());
  REQUIRE(validate_solution(p, Solution{{{1, 2}}, {3}}).empty());
  REQUIRE_FALSE(validate_solution(p, Solution{{{1, 2}}, {}}).empty());          // 3 missing
  REQUIRE_FALSE(validate_solution(p, Solution{{{1, 2, 2}}, {3}}).empty());      // duplicate
  REQUIRE_FALSE(validate_solution(p, Solution{{{1}, {2, 3}}, {}}).empty());     // 2 vehicles > 1
  REQUIRE_FALSE(validate_solution(p, Solution{{{1, 2, 3}, {}}, {}}).empty());   // empty route
}

TEST_CASE("solution_stats sums routes") {
  const Problem p = table_problem(kLine, {stop(1), stop(1), stop(1)}, Fleet{2, 10});
  const SolutionStats st = solution_stats(p, Solution{{{1}, {2, 3}}, {}});
  REQUIRE(st.km == Approx(20 + 60));
  REQUIRE(st.vehicles_used == 2);
  REQUIRE(st.served == 3);
}
