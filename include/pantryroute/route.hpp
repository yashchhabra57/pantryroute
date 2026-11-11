#pragma once

#include <string>
#include <vector>

#include "pantryroute/model.hpp"

namespace pantryroute {

/// A vehicle's ordered list of stop nodes. The depot is implicit at both ends.
using Route = std::vector<Node>;

struct Solution {
  std::vector<Route> routes;  // never contains empty routes
  std::vector<Node> unserved;  // stops no vehicle could reach in time / fit
};

struct Visit {
  Node node = kDepot;
  double arrival = 0;    // minutes after midnight
  double start = 0;      // service start (arrival, or window open if early)
  double departure = 0;  // start + service
  int load_after = 0;    // boxes still on board after this stop
};

enum class Violation { None, Capacity, TimeWindow, ShiftEnd };

struct RouteEval {
  Violation violation = Violation::None;
  Node violating_node = kDepot;  // where the first time window was missed
  double km = 0;
  double depart_depot = 0;
  double return_depot = 0;
  double wait = 0;  // total idle minutes waiting for windows to open
  int load = 0;     // boxes loaded at the depot
  std::vector<Visit> visits;  // filled only when a schedule is requested

  [[nodiscard]] bool feasible() const { return violation == Violation::None; }
  [[nodiscard]] double duration() const { return return_depot - depart_depot; }
};

/// Simulates driving `route`: leave the depot when it opens, wait if early,
/// and fail on the first late arrival, capacity overflow, or return after close.
RouteEval evaluate_route(const Problem& p, const Route& route, bool with_schedule = false);

/// Allocation-free feasibility check used in the solver's inner loops.
bool route_feasible(const Problem& p, const Route& route);

double route_km(const Problem& p, const Route& route);

struct SolutionStats {
  double km = 0;
  double drive_minutes = 0;  // sum of route durations (incl. service + waiting)
  std::size_t vehicles_used = 0;
  std::size_t served = 0;
  std::size_t unserved = 0;
};

SolutionStats solution_stats(const Problem& p, const Solution& s);

/// Independent checker: every stop appears exactly once (in a route or unserved),
/// no route is empty or infeasible, and the fleet size is respected.
/// Returns human-readable problems; empty means valid.
std::vector<std::string> validate_solution(const Problem& p, const Solution& s);

const char* to_string(Violation v);

}  // namespace pantryroute
