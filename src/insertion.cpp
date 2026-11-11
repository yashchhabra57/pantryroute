#include "pantryroute/insertion.hpp"

#include <algorithm>

namespace pantryroute {

std::optional<InsertionPoint> cheapest_insertion(const Problem& p, const std::vector<Route>& routes,
                                                 Node node) {
  std::optional<InsertionPoint> best;
  Route candidate;
  for (std::size_t r = 0; r < routes.size(); ++r) {
    const Route& route = routes[r];
    for (std::size_t pos = 0; pos <= route.size(); ++pos) {
      const Node before = pos == 0 ? kDepot : route[pos - 1];
      const Node after = pos == route.size() ? kDepot : route[pos];
      const double added = p.km(before, node) + p.km(node, after) - p.km(before, after);
      // Check the cheap distance bound first; only simulate if it would win.
      if (best && added >= best->added_km) continue;
      candidate = route;
      candidate.insert(candidate.begin() + static_cast<std::ptrdiff_t>(pos), node);
      if (route_feasible(p, candidate)) best = InsertionPoint{r, pos, added};
    }
  }
  return best;
}

bool insert_node(const Problem& p, std::vector<Route>& routes, Node node, bool allow_new_route) {
  const auto at = cheapest_insertion(p, routes, node);
  const bool can_open = allow_new_route && routes.size() < static_cast<std::size_t>(p.fleet.vehicles) &&
                        route_feasible(p, {node});
  if (can_open && (!at || at->added_km > 2 * p.km(kDepot, node))) {
    routes.push_back({node});
    return true;
  }
  if (!at) return false;
  Route& r = routes[at->route];
  r.insert(r.begin() + static_cast<std::ptrdiff_t>(at->position), node);
  return true;
}

void enforce_fleet(const Problem& p, Solution& s) {
  const auto limit = static_cast<std::size_t>(p.fleet.vehicles);
  while (s.routes.size() > limit) {
    // Smallest route first (fewest stops, then least load): least work to re-home.
    const auto smallest = std::min_element(s.routes.begin(), s.routes.end(), [&](const Route& a, const Route& b) {
      if (a.size() != b.size()) return a.size() < b.size();
      return evaluate_route(p, a).load < evaluate_route(p, b).load;
    });
    Route orphans = std::move(*smallest);
    s.routes.erase(smallest);
    // Re-home the farthest stops first; they are the hardest to fit.
    std::sort(orphans.begin(), orphans.end(),
              [&](Node a, Node b) { return p.km(kDepot, a) > p.km(kDepot, b); });
    for (const Node n : orphans) {
      if (!insert_node(p, s.routes, n, false)) s.unserved.push_back(n);
    }
  }
}

std::size_t reinsert_unserved(const Problem& p, Solution& s) {
  std::size_t placed = 0;
  std::vector<Node> still;
  for (const Node n : s.unserved) {
    if (insert_node(p, s.routes, n, true)) {
      ++placed;
    } else {
      still.push_back(n);
    }
  }
  std::sort(still.begin(), still.end());
  s.unserved = std::move(still);
  return placed;
}

}  // namespace pantryroute
