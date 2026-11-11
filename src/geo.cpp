#include "pantryroute/geo.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace pantryroute {

namespace {
constexpr double kEarthRadiusKm = 6371.0088;

double to_radians(double degrees) { return degrees * std::numbers::pi / 180.0; }
}  // namespace

double haversine_km(LatLon a, LatLon b) {
  const double dlat = to_radians(b.lat - a.lat);
  const double dlon = to_radians(b.lon - a.lon);
  const double s1 = std::sin(dlat / 2.0);
  const double s2 = std::sin(dlon / 2.0);
  const double h = s1 * s1 + std::cos(to_radians(a.lat)) * std::cos(to_radians(b.lat)) * s2 * s2;
  // Clamp guards against h drifting a hair above 1.0 for antipodal points.
  return 2.0 * kEarthRadiusKm * std::asin(std::sqrt(std::fmin(1.0, h)));
}

Matrix distance_matrix_km(const std::vector<LatLon>& points, double road_factor) {
  if (road_factor < 1.0) {
    throw std::invalid_argument("road factor must be >= 1.0");
  }
  Matrix m(points.size());
  for (std::size_t i = 0; i < points.size(); ++i) {
    for (std::size_t j = i + 1; j < points.size(); ++j) {
      const double d = haversine_km(points[i], points[j]) * road_factor;
      m.at(i, j) = d;
      m.at(j, i) = d;
    }
  }
  return m;
}

Matrix travel_minutes(const Matrix& km, double speed_kmh) {
  if (speed_kmh <= 0.0) {
    throw std::invalid_argument("speed must be positive");
  }
  Matrix out(km.size());
  for (std::size_t i = 0; i < km.size(); ++i) {
    for (std::size_t j = 0; j < km.size(); ++j) {
      out.at(i, j) = km(i, j) / speed_kmh * 60.0;
    }
  }
  return out;
}

}  // namespace pantryroute
