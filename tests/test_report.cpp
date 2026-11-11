#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "fixtures.hpp"
#include "pantryroute/report.hpp"

using namespace pantryroute;
using namespace pantryroute::testing;
using Catch::Matchers::ContainsSubstring;

namespace {
Problem small_problem() {
  Stop depot;
  depot.id = "HUB";
  depot.name = "Food Shelf & Co";
  depot.pos = {46.72, -92.10};
  depot.open = 9 * 60;
  depot.close = 15 * 60;
  std::vector<Stop> stops;
  auto add = [&](std::string id, double lat, double lon, int demand, int open, int close, std::string notes = "") {
    Stop s;
    s.id = id;
    s.name = "Home " + id;
    s.pos = {lat, lon};
    s.demand = demand;
    s.open = open;
    s.close = close;
    s.service = 5;
    s.notes = std::move(notes);
    stops.push_back(s);
  };
  add("A", 46.73, -92.11, 2, 540, 900, "Ring twice <side door>");
  add("B", 46.74, -92.08, 3, 600, 660);
  add("C", 46.71, -92.12, 99, 540, 900);  // too big for any vehicle
  return build_problem(depot, stops, Fleet{2, 10, 40.0, 1.3});
}
}  // namespace

TEST_CASE("unserved reasons explain the blocking constraint") {
  const Problem p = small_problem();
  REQUIRE_THAT(unserved_reason(p, 3), ContainsSubstring("needs 99 units"));
  REQUIRE_THAT(unserved_reason(p, 1), ContainsSubstring("no vehicle"));
}

TEST_CASE("plan JSON includes schedule, summary and unserved stops") {
  const Problem p = small_problem();
  const SolveReport r = solve(p);
  const std::string json = plan_json(p, r);
  REQUIRE_THAT(json, ContainsSubstring("\"routes\": ["));
  REQUIRE_THAT(json, ContainsSubstring("\"id\": \"B\""));
  REQUIRE_THAT(json, ContainsSubstring("\"window\": \"10:00-11:00\""));
  REQUIRE_THAT(json, ContainsSubstring("\"unserved\": ["));
  REQUIRE_THAT(json, ContainsSubstring("needs 99 units"));
  REQUIRE_THAT(json, ContainsSubstring("\"improvement_pct\""));
}

TEST_CASE("driver manifest lists stops in order with notes") {
  const Problem p = small_problem();
  const SolveReport r = solve(p);
  const std::string m = driver_manifest(p, r);
  REQUIRE_THAT(m, ContainsSubstring("==== Vehicle 1 ===="));
  REQUIRE_THAT(m, ContainsSubstring("Home A [A]"));
  REQUIRE_THAT(m, ContainsSubstring("note: Ring twice"));
  REQUIRE_THAT(m, ContainsSubstring("==== Not scheduled ===="));
}

TEST_CASE("svg map escapes names and draws every route") {
  const Problem p = small_problem();
  const SolveReport r = solve(p);
  const std::string svg = route_svg(p, r.solution);
  REQUIRE(svg.starts_with("<svg"));
  REQUIRE_THAT(svg, ContainsSubstring("<polyline"));
  REQUIRE_THAT(svg, ContainsSubstring("Food Shelf &amp; Co (depot)"));
  REQUIRE_THAT(svg, ContainsSubstring("not scheduled"));
  REQUIRE_THAT(svg, !ContainsSubstring("Shelf & Co"));
}

TEST_CASE("summary text reports both phases") {
  const Problem p = small_problem();
  const std::string text = summary_text(p, solve(p));
  REQUIRE_THAT(text, ContainsSubstring("Savings construction:"));
  REQUIRE_THAT(text, ContainsSubstring("After local search:"));
  REQUIRE_THAT(text, ContainsSubstring("Vehicle 1:"));
}
