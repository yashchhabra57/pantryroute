#pragma once

#include "pantryroute/route.hpp"

namespace pantryroute {

/// Number of improving moves accepted, per neighbourhood.
struct MoveCounts {
  std::size_t two_opt = 0;   // reverse a segment inside one route
  std::size_t relocate = 0;  // move one stop to another position / route
  std::size_t or_opt = 0;    // move a chain of 2-3 consecutive stops (optionally reversed)
  std::size_t swap = 0;      // exchange two stops between routes
  std::size_t cross = 0;     // 2-opt*: exchange the tails of two routes
  std::size_t rounds = 0;

  [[nodiscard]] std::size_t total() const { return two_opt + relocate + or_opt + swap + cross; }
};

// Every operator below only accepts a move that (1) strictly shortens total distance
// and (2) keeps every touched route feasible. Distance deltas are computed in O(1)
// from the edges that change, and the O(route length) feasibility simulation runs only
// for moves that would actually improve. Assumes a symmetric distance matrix.

/// Runs 2-opt on a single route until no improving reversal is left.
std::size_t improve_two_opt(const Problem& p, Route& route);

/// Applies the first improving relocate move found. Returns true if one was applied.
bool improve_relocate(const Problem& p, std::vector<Route>& routes);

/// Applies the first improving Or-opt move: a chain of 2-3 consecutive stops moved,
/// possibly reversed, to another position in the same or a different route.
bool improve_or_opt(const Problem& p, std::vector<Route>& routes);

/// Applies the first improving inter-route swap. Returns true if one was applied.
bool improve_swap(const Problem& p, std::vector<Route>& routes);

/// Applies the first improving tail exchange between two routes.
bool improve_cross(const Problem& p, std::vector<Route>& routes);

/// Repeats all neighbourhoods until a full round finds nothing (a local optimum)
/// or `max_rounds` is reached. Empty routes are removed.
MoveCounts local_search(const Problem& p, Solution& s, std::size_t max_rounds = 100);

}  // namespace pantryroute
