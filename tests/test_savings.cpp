#include <algorithm>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fixtures.hpp"
#include "pantryroute/savings.hpp"

using namespace pantryroute;
using namespace pantryroute::testing;
using Catch::Approx;

namespace {
// Two clusters on opposite sides of the depot:
//   1,2 are 10 east (2 apart); 3,4 are 10 west (2 apart); east<->west is 20.
const std::vector<std::vector<double>> kClusters{
    {0, 10, 10, 10, 10},
    {10, 0, 2, 20, 20},
    {10, 2, 0, 20, 20},
    {10, 20, 20, 0, 2},
    {10, 20, 20, 2, 0},
};
}  // namespace

TEST_CASE("merges nearby stops into shared routes") {
  const Problem p = table_problem(kClusters, {stop(1), stop(1), stop(1), stop(1)}, Fleet{4, 2});
  const Solution s = savings_construct(p);
  REQUIRE(validate_solution(p, s).empty());
  REQUIRE(s.routes.size() == 2);
  REQUIRE(solution_stats(p, s).km == Approx(2 * (10 + 2 + 10)));
}

TEST_CASE("capacity stops merges") {
  const Problem p = table_problem(kClusters, {stop(2), stop(2), stop(1), stop(1)}, Fleet{4, 2});
  const Solution s = savings_construct(p);
  REQUIRE(validate_solution(p, s).empty());
  REQUIRE(s.routes.size() == 3);  // 1 and 2 each fill a vehicle
}

TEST_CASE("orients merged routes to respect time windows") {
  // Stop 2 must be served first (closes at 10), stop 1 later (opens at 30).
  const Problem p = table_problem(kClusters, {stop(1, 30, 1440), stop(1, 0, 10), stop(1), stop(1)},
                                  Fleet{4, 10});
  const Solution s = savings_construct(p);
  REQUIRE(validate_solution(p, s).empty());
  bool found = false;
  for (const Route& r : s.routes) {
    const auto it1 = std::find(r.begin(), r.end(), Node{1});
    const auto it2 = std::find(r.begin(), r.end(), Node{2});
    if (it1 != r.end() && it2 != r.end()) {
      found = true;
      REQUIRE(it2 < it1);
    }
  }
  REQUIRE(found);
}

TEST_CASE("stops impossible on their own are unserved") {
  // Stop 3 closes before you can even drive there.
  const Problem p = table_problem(kClusters, {stop(1), stop(1), stop(1, 0, 5), stop(50)}, Fleet{4, 10});
  const Solution s = savings_construct(p);
  REQUIRE(validate_solution(p, s).empty());
  REQUIRE(s.unserved == std::vector<Node>{3, 4});
}

TEST_CASE("random instances always produce valid solutions") {
  for (unsigned seed = 1; seed <= 20; ++seed) {
    const Problem p = random_problem(seed, 40, Fleet{40, 20});
    const Solution s = savings_construct(p);
    INFO("seed " << seed);
    REQUIRE(validate_solution(p, s).empty());
    // Merging should beat one-vehicle-per-stop by a wide margin.
    REQUIRE(s.routes.size() < 25);
  }
}
