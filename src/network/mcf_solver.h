#pragma once

#include "network/network_data.h"
#include "network/network_solution.h"

namespace coso {

/// Minimum cost flow solver using successive shortest paths (SSP).
///
/// Computes an optimal or near-optimal MCF solution by repeatedly finding
/// shortest augmenting paths from supply nodes to demand nodes in the
/// residual graph using Bellman-Ford (handles negative costs).
///
/// The solver handles basic MCF without resource constraints. For RCMCF,
/// combine with the network operators for local search improvement.
class McfSolver {
public:
    /// Solve the minimum cost flow problem.
    /// Returns a feasible, cost-minimising flow if one exists.
    /// If no feasible flow exists (supply/demand imbalance, or capacity
    /// constraints prevent routing all flow), the returned solution will
    /// have excess violations.
    [[nodiscard]] static NetworkSolution solve(NetworkData const& data);

private:
    /// Find a shortest path from src to dst in the residual graph
    /// using Bellman-Ford. Returns true if a path exists, and fills
    /// pred_arc with the predecessor arc indices (in the residual graph).
    /// dist[n] contains the shortest distance to node n.
    ///
    /// The residual graph has:
    ///   - Forward arcs (a) with residual = upper_cap - flow, cost = cost
    ///   - Backward arcs (a + num_arcs) with residual = flow - lower_cap,
    ///     cost = -cost
    struct PathResult {
        bool found = false;
        int bottleneck = 0;         ///< max flow that can be sent
        std::vector<int> pred_arc;  ///< predecessor arc (-1 = none)
        std::vector<long long> dist;
    };

    [[nodiscard]] static PathResult find_shortest_path(
        NetworkData const& data,
        NetworkSolution const& sol,
        int src, int dst);
};

} // namespace coso
