#pragma once

#include <optional>

#include "pantryroute/route.hpp"

namespace pantryroute {

struct InsertionPoint {
  std::size_t route = 0;     // index into the routes vector
  std::size_t position = 0;  // insert before routes[route][position]
  double added_km = 0;
};

/// Cheapest feasible place to insert `node` into any existing route.
std::optional<InsertionPoint> cheapest_insertion(const Problem& p, const std::vector<Route>& routes,
                                                 Node node);

/// Inserts `node` at its cheapest feasible position, or opens a new route when
/// `allow_new_route` is set and a vehicle is free. Returns false if neither works.
bool insert_node(const Problem& p, std::vector<Route>& routes, Node node, bool allow_new_route);

/// Brings a solution within the fleet size: while there are more routes than
/// vehicles, dissolve the smallest route and re-insert its stops elsewhere.
/// Stops that fit nowhere become unserved.
void enforce_fleet(const Problem& p, Solution& s);

/// Tries to place unserved stops into the plan (existing routes or spare vehicles).
/// Returns how many were placed.
std::size_t reinsert_unserved(const Problem& p, Solution& s);

}  // namespace pantryroute
