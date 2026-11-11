#pragma once

#include "pantryroute/local_search.hpp"
#include "pantryroute/route.hpp"

namespace pantryroute {

struct SolveOptions {
  bool local_search = true;
  std::size_t max_rounds = 200;
};

struct SolveReport {
  Solution solution;
  SolutionStats construction;  // after savings + fleet repair, before local search
  SolutionStats final;
  MoveCounts moves;
  double elapsed_ms = 0;
};

/// Full pipeline:
///   1. Clarke-Wright savings (time-window aware) builds initial routes.
///   2. Fleet repair dissolves routes until they fit the available vehicles.
///   3. Local search (2-opt, relocate, swap, 2-opt*) shortens the plan.
///   4. Unserved stops get another chance now that routes are tighter; if any are
///      placed, local search runs again.
/// The result is deterministic for a given input.
SolveReport solve(const Problem& p, const SolveOptions& options = {});

}  // namespace pantryroute
