#include "pantryroute/route.hpp"

#include <algorithm>

namespace pantryroute {

namespace {
// Small tolerance so floating-point travel times don't reject a route that
// arrives "exactly" at closing time.
constexpr double kEps = 1e-6;
}  // namespace

RouteEval evaluate_route(const Problem& p, const Route& route, bool with_schedule) {
  RouteEval ev;
  for (const Node n : route) ev.load += p.node(n).demand;
  if (ev.load > p.fleet.capacity) ev.violation = Violation::Capacity;

  ev.depart_depot = p.depot.open;
  double t = ev.depart_depot;
  int on_board = ev.load;
  Node prev = kDepot;
  if (with_schedule) ev.visits.reserve(route.size());

  for (const Node n : route) {
    const Stop& s = p.node(n);
    const double arrival = t + p.minutes(prev, n);
    const double start = std::max(arrival, static_cast<double>(s.open));
    if (start > s.close + kEps && ev.violation == Violation::None) {
      ev.violation = Violation::TimeWindow;
      ev.violating_node = n;
    }
    ev.wait += start - arrival;
    ev.km += p.km(prev, n);
    t = start + s.service;
    on_board -= s.demand;
    if (with_schedule) ev.visits.push_back({n, arrival, start, t, on_board});
    prev = n;
  }
  ev.km += p.km(prev, kDepot);
  ev.return_depot = t + p.minutes(prev, kDepot);
  if (ev.return_depot > p.depot.close + kEps && ev.violation == Violation::None) {
    ev.violation = Violation::ShiftEnd;
  }
  return ev;
}

bool route_feasible(const Problem& p, const Route& route) {
  int load = 0;
  for (const Node n : route) load += p.node(n).demand;
  if (load > p.fleet.capacity) return false;

  double t = p.depot.open;
  Node prev = kDepot;
  for (const Node n : route) {
    const Stop& s = p.node(n);
    t = std::max(t + p.minutes(prev, n), static_cast<double>(s.open));
    if (t > s.close + kEps) return false;
    t += s.service;
    prev = n;
  }
  return t + p.minutes(prev, kDepot) <= p.depot.close + kEps;
}

double route_km(const Problem& p, const Route& route) {
  double km = 0;
  Node prev = kDepot;
  for (const Node n : route) {
    km += p.km(prev, n);
    prev = n;
  }
  return km + p.km(prev, kDepot);
}

SolutionStats solution_stats(const Problem& p, const Solution& s) {
  SolutionStats st;
  for (const Route& r : s.routes) {
    const RouteEval ev = evaluate_route(p, r);
    st.km += ev.km;
    st.drive_minutes += ev.duration();
    st.served += r.size();
  }
  st.vehicles_used = s.routes.size();
  st.unserved = s.unserved.size();
  return st;
}

std::vector<std::string> validate_solution(const Problem& p, const Solution& s) {
  std::vector<std::string> problems;
  std::vector<int> seen(p.node_count(), 0);
  auto mark = [&](Node n) {
    if (n == kDepot || n >= p.node_count()) {
      problems.push_back("invalid node index " + std::to_string(n));
      return;
    }
    ++seen[n];
  };
  for (std::size_t i = 0; i < s.routes.size(); ++i) {
    const Route& r = s.routes[i];
    if (r.empty()) problems.push_back("route " + std::to_string(i + 1) + " is empty");
    for (const Node n : r) mark(n);
    if (!r.empty() && std::all_of(r.begin(), r.end(), [&](Node n) { return n > 0 && n < p.node_count(); })) {
      const RouteEval ev = evaluate_route(p, r);
      if (!ev.feasible()) {
        problems.push_back("route " + std::to_string(i + 1) + " violates " + to_string(ev.violation));
      }
    }
  }
  for (const Node n : s.unserved) mark(n);
  for (Node n = 1; n < p.node_count(); ++n) {
    if (seen[n] != 1) {
      problems.push_back("stop '" + p.node(n).id + "' appears " + std::to_string(seen[n]) + " times");
    }
  }
  if (s.routes.size() > static_cast<std::size_t>(p.fleet.vehicles)) {
    problems.push_back("uses " + std::to_string(s.routes.size()) + " vehicles but only " +
                       std::to_string(p.fleet.vehicles) + " are available");
  }
  return problems;
}

const char* to_string(Violation v) {
  switch (v) {
    case Violation::None: return "none";
    case Violation::Capacity: return "capacity";
    case Violation::TimeWindow: return "time window";
    case Violation::ShiftEnd: return "shift end";
  }
  return "unknown";
}

}  // namespace pantryroute
