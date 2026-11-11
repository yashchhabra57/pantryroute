#pragma once

#include <cstddef>
#include <vector>

namespace pantryroute {

struct LatLon {
  double lat = 0.0;
  double lon = 0.0;
};

/// Great-circle distance in kilometres (haversine formula, mean Earth radius).
double haversine_km(LatLon a, LatLon b);

/// Square, row-major matrix of travel costs between locations.
class Matrix {
 public:
  Matrix() = default;
  explicit Matrix(std::size_t n, double fill = 0.0) : n_(n), data_(n * n, fill) {}

  [[nodiscard]] std::size_t size() const { return n_; }
  [[nodiscard]] double operator()(std::size_t from, std::size_t to) const {
    return data_[from * n_ + to];
  }
  double& at(std::size_t from, std::size_t to) { return data_[from * n_ + to]; }

 private:
  std::size_t n_ = 0;
  std::vector<double> data_;
};

/// Builds a road-distance estimate: haversine distance scaled by `road_factor`
/// (straight lines underestimate real driving; ~1.3 is a common urban rule of thumb).
Matrix distance_matrix_km(const std::vector<LatLon>& points, double road_factor);

/// Converts a distance matrix (km) into travel minutes at a constant speed.
Matrix travel_minutes(const Matrix& km, double speed_kmh);

}  // namespace pantryroute
