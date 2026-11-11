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

TEST_CASE("still waits when an earlier window prevents leaving later") {
  // Stop 1 must start by 15; stop 2 opens at 100. Leaving at 0 reaches 1 at 10, so
  // the van can leave at most 5 minutes late and still has to wait at stop 2.
  const Problem p = table_problem(kLine, {stop(1, 0, 15), stop(1, 100, 200), stop(1)}, Fleet{1, 10});
  const RouteEval ev = evaluate_route(p, {1, 2}, true);
  REQUIRE(ev.feasible());
  REQUIRE(ev.depart_depot == Approx(5));
  REQUIRE(ev.visits[0].start == Approx(15));
  REQUIRE(ev.visits[1].arrival == Approx(25));
  REQUIRE(ev.visits[1].start == Approx(100));
  REQUIRE(ev.wait == Approx(75));
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

TEST_CASE("leaves the depot late enough to skip idle waiting") {
  // Stop 1 opens at 600 but is 10 minutes away from a depot that opens at 480.
  const Problem p = table_problem(kLine, {stop(1, 600, 700), stop(1, 0, 1440), stop(1)}, Fleet{1, 10}, 480, 1080);
  const RouteEval ev = evaluate_route(p, {1, 2}, true);
  REQUIRE(ev.feasible());
  REQUIRE(ev.depart_depot == Approx(590));
  REQUIRE(ev.wait == Approx(0));
  REQUIRE(ev.visits[0].start == Approx(600));
  REQUIRE(ev.duration() == Approx(40));
}

TEST_CASE("departure delay is limited by a tight later window") {
  // Stop 1 opens late (600) but stop 2 must start by 615, 10 minutes after stop 1.
  // Leaving at opening: stop 1 at 490 -> wait 110 -> 600, stop 2 at 610 (5 min slack).
  // Delay is therefore capped at 110 (the wait), and stop 2 still makes it at 610.
  const Problem p = table_problem(kLine, {stop(1, 600, 700), stop(1, 0, 615), stop(1)}, Fleet{1, 10}, 480, 1080);
  const RouteEval ev = evaluate_route(p, {1, 2}, true);
  REQUIRE(ev.feasible());
  REQUIRE(ev.depart_depot == Approx(590));
  REQUIRE(ev.visits[1].start <= 615);

  // Here stop 2 (visited first) closes at 25, so the van must leave at opening.
  const Problem tight = table_problem(kLine, {stop(1, 100, 200), stop(1, 0, 25), stop(1)}, Fleet{1, 10}, 0, 1080);
  const RouteEval ev2 = evaluate_route(tight, {2, 1}, true);
  REQUIRE(ev2.feasible());
  REQUIRE(ev2.depart_depot == Approx(5));  // 20 min drive, must start by 25
  REQUIRE(ev2.visits[0].start == Approx(25));
}

TEST_CASE("departure shift never changes feasibility or distance") {
  const Problem p = random_problem(11, 30, Fleet{4, 25}, 0.6);
  std::mt19937 rng(5);
  for (int trial = 0; trial < 300; ++trial) {
    Route r;
    for (Node n = 1; n < p.node_count(); ++n) {
      if (rng() % 5 == 0) r.push_back(n);
    }
    std::shuffle(r.begin(), r.end(), rng);
    const RouteEval ev = evaluate_route(p, r, true);
    REQUIRE(ev.feasible() == route_feasible(p, r));
    REQUIRE(ev.km == Approx(route_km(p, r)));
    if (ev.feasible()) {
      REQUIRE(ev.depart_depot >= p.depot.open);
      for (const Visit& v : ev.visits) REQUIRE(v.start <= p.node(v.node).close + 1e-6);
      REQUIRE(ev.return_depot <= p.depot.close + 1e-6);
    }
  }
}
