#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pantryroute/loader.hpp"

using namespace pantryroute;
using Catch::Matchers::ContainsSubstring;

namespace {
bool any_error_contains(const LoadResult& r, const std::string& needle) {
  for (const auto& e : r.errors) {
    if (e.find(needle) != std::string::npos) return true;
  }
  return false;
}
}  // namespace

TEST_CASE("loads depot and stops with kind column in any order") {
  const auto r = load_stops_csv(
      "lon,lat,id,kind,name,demand,open,close,service\n"
      "-92.10,46.72,A,stop,Ann,3,10:00,12:00,5\n"
      "-92.09,46.71,HUB,depot,Food Shelf,,08:00,17:00,\n"
      "-92.05,46.70,B,stop,,2,,,\n");
  REQUIRE(r.ok());
  const auto& d = *r.data;
  REQUIRE(d.depot.id == "HUB");
  REQUIRE(d.depot.open == 480);
  REQUIRE(d.stops.size() == 2);
  REQUIRE(d.stops[0].demand == 3);
  REQUIRE(d.stops[0].open == 600);
  REQUIRE(d.stops[0].service == 5);
  // B inherits depot hours and falls back to its id for a name.
  REQUIRE(d.stops[1].open == 480);
  REQUIRE(d.stops[1].close == 1020);
  REQUIRE(d.stops[1].name == "B");
}

TEST_CASE("first row is the depot when there is no kind column") {
  const auto r = load_stops_csv("id,lat,lon\nHUB,46.7,-92.1\nA,46.8,-92.0\n");
  REQUIRE(r.ok());
  REQUIRE(r.data->depot.id == "HUB");
  REQUIRE(r.data->stops.size() == 1);
}

TEST_CASE("reports every row problem with line numbers") {
  const auto r = load_stops_csv(
      "id,kind,lat,lon,demand,open,close\n"
      "HUB,depot,46.7,-92.1,,08:00,17:00\n"
      "A,stop,95,-92.0,2,,\n"
      "A,stop,46.7,-92.0,-1,,\n"
      "C,truck,46.7,-92.0,1,13:00,9:00\n"
      "D,stop,46.7,abc,1,9am,\n");
  REQUIRE_FALSE(r.ok());
  CHECK(any_error_contains(r, "line 3: lat"));
  CHECK(any_error_contains(r, "line 4: duplicate id 'A'"));
  CHECK(any_error_contains(r, "line 4: demand"));
  CHECK(any_error_contains(r, "line 5: kind"));
  CHECK(any_error_contains(r, "line 5: open is after close"));
  CHECK(any_error_contains(r, "line 6: lon"));
  CHECK(any_error_contains(r, "line 6: open must be HH:MM"));
}

TEST_CASE("header problems are reported before rows") {
  const auto r = load_stops_csv("id,lat\nHUB,46.7\n");
  REQUIRE_FALSE(r.ok());
  REQUIRE_THAT(r.errors.at(0), ContainsSubstring("missing required column 'lon'"));
}

TEST_CASE("requires exactly one depot and at least one stop") {
  CHECK(any_error_contains(load_stops_csv("id,kind,lat,lon\nA,stop,46,-92\n"), "exactly one depot"));
  CHECK(any_error_contains(load_stops_csv("id,kind,lat,lon\nA,depot,46,-92\nB,depot,46,-92\n"),
                           "exactly one depot"));
  CHECK(any_error_contains(load_stops_csv("id,kind,lat,lon\nA,depot,46,-92\n"), "no delivery stops"));
  CHECK(any_error_contains(load_stops_csv("id,lat,lon\n"), "no data rows"));
  CHECK(any_error_contains(load_stops_csv(""), "empty"));
}

TEST_CASE("CSV syntax errors surface as load errors") {
  const auto r = load_stops_csv("id,lat,lon\n\"HUB,46,-92\n");
  REQUIRE_FALSE(r.ok());
  REQUIRE_THAT(r.errors.at(0), ContainsSubstring("unterminated"));
}

TEST_CASE("build_problem precomputes matrices with the depot at node 0") {
  const auto r = load_stops_csv("id,lat,lon\nHUB,46.70,-92.10\nA,46.80,-92.10\n");
  REQUIRE(r.ok());
  const Problem p = build_problem(r.data->depot, r.data->stops, Fleet{2, 10, 60.0, 1.0});
  REQUIRE(p.node_count() == 2);
  REQUIRE(p.node(kDepot).id == "HUB");
  REQUIRE(p.node(1).id == "A");
  // 0.1 degree of latitude is ~11.1 km; at 60 km/h that is ~11.1 minutes.
  REQUIRE(p.km(0, 1) > 11.0);
  REQUIRE(p.km(0, 1) < 11.3);
  REQUIRE(p.minutes(0, 1) == p.km(0, 1));
  REQUIRE_THROWS(build_problem(r.data->depot, r.data->stops, Fleet{0, 10, 60.0, 1.0}));
}
