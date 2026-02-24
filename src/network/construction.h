#pragma once

#include "network/network_data.h"
#include "network/network_solution.h"

namespace coso {

/// Greedy construction: send flow along shortest paths from supply to demand
/// nodes, one unit at a time (or as much as the bottleneck allows).
///
/// Uses BFS/Dijkstra to find shortest paths in the residual graph and sends
/// as much flow as possible along each path. Simpler and faster than the
/// full MCF solver but may not find the optimal solution.
[[nodiscard]] NetworkSolution construct_greedy(NetworkData const& data);

/// Compute an initial feasible flow by satisfying lower bounds and then
/// routing remaining supply along shortest available paths.
///
/// Steps:
/// 1. Set flow on each arc to its lower bound (mandatory flow).
/// 2. Route remaining excess using greedy shortest-path augmentation.
[[nodiscard]] NetworkSolution construct_feasible(NetworkData const& data);

} // namespace coso
