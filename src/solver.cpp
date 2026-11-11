#include "pantryroute/solver.hpp"

#include <algorithm>
#include <chrono>

#include "pantryroute/insertion.hpp"
#include "pantryroute/savings.hpp"

namespace pantryroute {

namespace {

void add_moves(MoveCounts& into, const MoveCounts& more) {
  into.two_opt += more.two_opt;
  into.relocate += more.relocate;
  into.swap += more.swap;
  into.cross += more.cross;
  into.rounds += more.rounds;
}

}  // namespace

SolveReport solve(const Problem& p, const SolveOptions& options) {
  const auto started = std::chrono::steady_clock::now();
  SolveReport report;

  Solution s = savings_construct(p);
  enforce_fleet(p, s);
  report.construction = solution_stats(p, s);

  if (options.local_search) {
    add_moves(report.moves, local_search(p, s, options.max_rounds));
    if (!s.unserved.empty() && reinsert_unserved(p, s) > 0) {
      add_moves(report.moves, local_search(p, s, options.max_rounds));
    }
  } else {
    reinsert_unserved(p, s);
  }
  std::sort(s.unserved.begin(), s.unserved.end());

  // Present routes in a stable order: earliest-starting first stop, then id.
  std::sort(s.routes.begin(), s.routes.end(), [&](const Route& a, const Route& b) {
    if (p.node(a.front()).open != p.node(b.front()).open) return p.node(a.front()).open < p.node(b.front()).open;
    return a.front() < b.front();
  });

  report.final = solution_stats(p, s);
  report.solution = std::move(s);
  report.elapsed_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  return report;
}

}  // namespace pantryroute
