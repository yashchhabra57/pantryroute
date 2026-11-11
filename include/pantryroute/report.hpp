#pragma once

#include <string>

#include "pantryroute/solver.hpp"

namespace pantryroute {

/// Why a stop ended up unserved, in words a coordinator can act on.
std::string unserved_reason(const Problem& p, Node n);

/// Machine-readable plan: summary, per-route schedule and unserved stops.
std::string plan_json(const Problem& p, const SolveReport& report);

/// Printable driver sheets: one block per route with arrival times, loads and notes.
std::string driver_manifest(const Problem& p, const SolveReport& report);

/// One-screen summary for the terminal.
std::string summary_text(const Problem& p, const SolveReport& report);

/// Self-contained SVG map (equirectangular projection) of the depot, stops and routes.
std::string route_svg(const Problem& p, const Solution& s, int width = 900, int height = 700);

}  // namespace pantryroute
