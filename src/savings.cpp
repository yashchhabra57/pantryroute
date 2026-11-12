#include "pantryroute/savings.hpp"

#include <algorithm>
#include <optional>

namespace pantryroute {

namespace {

struct Saving {
  Node i = kDepot;
  Node j = kDepot;
  double value = 0.0;
};

Route concat(const Route& a, const Route& b) {
  Route out;
  out.reserve(a.size() + b.size());
  out.insert(out.end(), a.begin(), a.end());
  out.insert(out.end(), b.begin(), b.end());
  return out;
}

Route reversed(const Route& r) { return Route(r.rbegin(), r.rend()); }

}  // namespace

Solution savings_construct(const Problem& p) {
  const std::size_t n = p.node_count();
  Solution sol;

  // routes[k] is alive while route_of points at it; merged-away routes are cleared.
  std::vector<Route> routes;
  std::vector<std::optional<std::size_t>> route_of(n);
  for (Node v = 1; v < n; ++v) {
    if (route_feasible(p, {v})) {
      route_of[v] = routes.size();
      routes.push_back({v});
    } else {
      sol.unserved.push_back(v);
    }
  }

  std::vector<Saving> savings;
  savings.reserve(routes.size() * routes.size() / 2);
  for (Node i = 1; i < n; ++i) {
    if (!route_of[i]) continue;
    for (Node j = i + 1; j < n; ++j) {
      if (!route_of[j]) continue;
      const double s = p.km(kDepot, i) + p.km(kDepot, j) - p.km(i, j);
      if (s > 0) savings.push_back({i, j, s});
    }
  }
  // Tie-break on node ids so the result is deterministic across platforms.
  std::sort(savings.begin(), savings.end(), [](const Saving& a, const Saving& b) {
    if (a.value != b.value) return a.value > b.value;
    if (a.i != b.i) return a.i < b.i;
    return a.j < b.j;
  });

  for (const Saving& s : savings) {
    const std::size_t ri = *route_of[s.i];
    const std::size_t rj = *route_of[s.j];
    if (ri == rj) continue;
    const Route& a = routes[ri];
    const Route& b = routes[rj];
    // Only route ends can be joined (an interior stop already has two neighbours).
    const bool i_head = a.front() == s.i;
    const bool i_tail = a.back() == s.i;
    const bool j_head = b.front() == s.j;
    const bool j_tail = b.back() == s.j;
    if (!(i_head || i_tail) || !(j_head || j_tail)) continue;

    // Orient both routes so i is a's tail and j is b's head; with time windows the
    // direction matters, so try both overall directions of the joined route.
    const Route a_fwd = i_tail ? a : reversed(a);
    const Route b_fwd = j_head ? b : reversed(b);
    std::optional<Route> merged;
    for (Route candidate : {concat(a_fwd, b_fwd), reversed(concat(a_fwd, b_fwd))}) {
      if (route_feasible(p, candidate)) {
        merged = std::move(candidate);
        break;
      }
    }
    if (!merged) continue;

    for (const Node v : routes[rj]) route_of[v] = ri;
    routes[rj].clear();
    routes[ri] = std::move(*merged);
  }

  for (auto& r : routes) {
    if (!r.empty()) sol.routes.push_back(std::move(r));
  }
  return sol;
}

}  // namespace pantryroute
