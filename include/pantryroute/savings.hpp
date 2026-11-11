#pragma once

#include "pantryroute/route.hpp"

namespace pantryroute {

/// Clarke-Wright savings construction, adapted for time windows.
///
/// Starts with one out-and-back route per stop, then repeatedly merges the pair of
/// routes whose end stops give the biggest saving s(i,j) = d(0,i) + d(0,j) - d(i,j),
/// as long as the merged route still passes `route_feasible`. Stops that can't be
/// served even on their own route go straight to `unserved`.
/// Does not enforce the fleet size; see `enforce_fleet`.
Solution savings_construct(const Problem& p);

}  // namespace pantryroute
