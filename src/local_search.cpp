#include "pantryroute/local_search.hpp"

#include <algorithm>

namespace pantryroute {

namespace {

constexpr double kImprove = -1e-9;

// Node before / after position `i` of a route, treating the depot as both ends.
Node before(const Route& r, std::size_t i) { return i == 0 ? kDepot : r[i - 1]; }
Node after(const Route& r, std::size_t i) { return i + 1 >= r.size() ? kDepot : r[i + 1]; }
Node at_or_depot(const Route& r, std::size_t i) { return i >= r.size() ? kDepot : r[i]; }

auto offset(std::size_t i) { return static_cast<std::ptrdiff_t>(i); }

void drop_empty(std::vector<Route>& routes) {
  std::erase_if(routes, [](const Route& r) { return r.empty(); });
}

}  // namespace

std::size_t improve_two_opt(const Problem& p, Route& route) {
  std::size_t applied = 0;
  bool improved = true;
  Route candidate;
  while (improved) {
    improved = false;
    for (std::size_t i = 0; i + 1 < route.size() && !improved; ++i) {
      for (std::size_t j = i + 1; j < route.size(); ++j) {
        // Reversing route[i..j] swaps edges (a,b),(c,d) for (a,c),(b,d).
        const Node a = before(route, i);
        const Node b = route[i];
        const Node c = route[j];
        const Node d = after(route, j);
        const double delta = p.km(a, c) + p.km(b, d) - p.km(a, b) - p.km(c, d);
        if (delta >= kImprove) continue;
        candidate = route;
        std::reverse(candidate.begin() + offset(i), candidate.begin() + offset(j) + 1);
        if (!route_feasible(p, candidate)) continue;
        route.swap(candidate);
        ++applied;
        improved = true;
        break;
      }
    }
  }
  return applied;
}

bool improve_relocate(const Problem& p, std::vector<Route>& routes) {
  Route from_new;
  Route to_new;
  for (std::size_t ra = 0; ra < routes.size(); ++ra) {
    for (std::size_t i = 0; i < routes[ra].size(); ++i) {
      const Route& a = routes[ra];
      const Node n = a[i];
      const double removal_gain = p.km(before(a, i), n) + p.km(n, after(a, i)) - p.km(before(a, i), after(a, i));

      for (std::size_t rb = 0; rb < routes.size(); ++rb) {
        // Within the same route, positions refer to the route with `n` taken out.
        Route reduced;
        if (rb == ra) {
          reduced = a;
          reduced.erase(reduced.begin() + offset(i));
        }
        const Route& b = rb == ra ? reduced : routes[rb];
        for (std::size_t pos = 0; pos <= b.size(); ++pos) {
          if (rb == ra && pos == i) continue;  // same place
          const Node x = before(b, pos);
          const Node y = at_or_depot(b, pos);
          const double delta = p.km(x, n) + p.km(n, y) - p.km(x, y) - removal_gain;
          if (delta >= kImprove) continue;

          to_new = b;
          to_new.insert(to_new.begin() + offset(pos), n);
          if (!route_feasible(p, to_new)) continue;
          if (rb == ra) {
            routes[ra] = std::move(to_new);
            return true;
          }
          from_new = a;
          from_new.erase(from_new.begin() + offset(i));
          if (!from_new.empty() && !route_feasible(p, from_new)) continue;
          routes[ra] = std::move(from_new);
          routes[rb] = std::move(to_new);
          drop_empty(routes);
          return true;
        }
      }
    }
  }
  return false;
}

bool improve_swap(const Problem& p, std::vector<Route>& routes) {
  for (std::size_t ra = 0; ra < routes.size(); ++ra) {
    for (std::size_t rb = ra + 1; rb < routes.size(); ++rb) {
      Route& a = routes[ra];
      Route& b = routes[rb];
      for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t j = 0; j < b.size(); ++j) {
          const Node u = a[i];
          const Node v = b[j];
          const Node ap = before(a, i);
          const Node an = after(a, i);
          const Node bp = before(b, j);
          const Node bn = after(b, j);
          const double delta = (p.km(ap, v) + p.km(v, an) - p.km(ap, u) - p.km(u, an)) +
                               (p.km(bp, u) + p.km(u, bn) - p.km(bp, v) - p.km(v, bn));
          if (delta >= kImprove) continue;
          std::swap(a[i], b[j]);
          if (route_feasible(p, a) && route_feasible(p, b)) return true;
          std::swap(a[i], b[j]);  // undo
        }
      }
    }
  }
  return false;
}

bool improve_cross(const Problem& p, std::vector<Route>& routes) {
  Route a_new;
  Route b_new;
  for (std::size_t ra = 0; ra < routes.size(); ++ra) {
    for (std::size_t rb = ra + 1; rb < routes.size(); ++rb) {
      const Route& a = routes[ra];
      const Route& b = routes[rb];
      // Cut a before position i and b before position j, then swap the tails.
      for (std::size_t i = 0; i <= a.size(); ++i) {
        for (std::size_t j = 0; j <= b.size(); ++j) {
          if ((i == 0 && j == 0) || (i == a.size() && j == b.size())) continue;  // no-op
          const Node ap = before(a, i);
          const Node ai = at_or_depot(a, i);
          const Node bp = before(b, j);
          const Node bj = at_or_depot(b, j);
          const double delta = p.km(ap, bj) + p.km(bp, ai) - p.km(ap, ai) - p.km(bp, bj);
          if (delta >= kImprove) continue;

          a_new.assign(a.begin(), a.begin() + offset(i));
          a_new.insert(a_new.end(), b.begin() + offset(j), b.end());
          b_new.assign(b.begin(), b.begin() + offset(j));
          b_new.insert(b_new.end(), a.begin() + offset(i), a.end());
          if ((!a_new.empty() && !route_feasible(p, a_new)) || (!b_new.empty() && !route_feasible(p, b_new))) {
            continue;
          }
          routes[ra] = std::move(a_new);
          routes[rb] = std::move(b_new);
          drop_empty(routes);
          return true;
        }
      }
    }
  }
  return false;
}

MoveCounts local_search(const Problem& p, Solution& s, std::size_t max_rounds) {
  MoveCounts counts;
  drop_empty(s.routes);
  while (counts.rounds < max_rounds) {
    ++counts.rounds;
    const std::size_t before_round = counts.total();
    for (Route& r : s.routes) counts.two_opt += improve_two_opt(p, r);
    while (improve_relocate(p, s.routes)) ++counts.relocate;
    while (improve_swap(p, s.routes)) ++counts.swap;
    while (improve_cross(p, s.routes)) ++counts.cross;
    if (counts.total() == before_round) break;
  }
  return counts;
}

}  // namespace pantryroute
