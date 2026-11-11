#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "pantryroute/geo.hpp"

using namespace pantryroute;
using Catch::Approx;

TEST_CASE("haversine is zero for identical points") {
  REQUIRE(haversine_km({46.72, -92.10}, {46.72, -92.10}) == Approx(0.0).margin(1e-12));
}

TEST_CASE("haversine matches a known city pair") {
  // Superior, WI to Minneapolis, MN is ~220 km as the crow flies.
  const double d = haversine_km({46.7208, -92.1041}, {44.9778, -93.2650});
  REQUIRE(d == Approx(213.0).margin(8.0));
}

TEST_CASE("haversine is symmetric") {
  const LatLon a{46.0, -92.0};
  const LatLon b{47.5, -90.25};
  REQUIRE(haversine_km(a, b) == Approx(haversine_km(b, a)));
}

TEST_CASE("distance matrix applies road factor and is symmetric") {
  const std::vector<LatLon> pts{{46.70, -92.10}, {46.75, -92.00}, {46.80, -92.20}};
  const Matrix km = distance_matrix_km(pts, 1.3);
  REQUIRE(km.size() == 3);
  for (std::size_t i = 0; i < 3; ++i) {
    REQUIRE(km(i, i) == 0.0);
    for (std::size_t j = 0; j < 3; ++j) REQUIRE(km(i, j) == km(j, i));
  }
  REQUIRE(km(0, 1) == Approx(haversine_km(pts[0], pts[1]) * 1.3));
}

TEST_CASE("road factor below one is rejected") {
  REQUIRE_THROWS_AS(distance_matrix_km({{0, 0}}, 0.9), std::invalid_argument);
}

TEST_CASE("travel minutes converts at constant speed") {
  Matrix km(2);
  km.at(0, 1) = 30.0;
  km.at(1, 0) = 30.0;
  const Matrix t = travel_minutes(km, 60.0);
  REQUIRE(t(0, 1) == Approx(30.0));
  REQUIRE_THROWS_AS(travel_minutes(km, 0.0), std::invalid_argument);
}
