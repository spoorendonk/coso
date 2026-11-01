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
/// The solver handles basic MCF. No search component in src/network/ produces
/// resource-feasible flows for the aggregate resource budgets NetworkData can
/// carry -- this solver and the operators are both resource-blind, and
/// NetworkSolution::resource_feasible() only checks them after the fact.
/// RCMCF belongs to #184's design (see #195).
class McfSolver {
public:
    /// Solve the minimum cost flow problem.
    /// Returns a feasible, cost-minimising flow if one exists.
    /// If no feasible flow exists (supply/demand imbalance, or capacity
    /// constraints prevent routing all flow), the returned solution will
    /// have excess violations.
    [[nodiscard]] static NetworkSolution solve(NetworkData const& data);

private:
    /// Find a shortest path in the residual graph from any node with positive
    /// excess to the nearest node with negative excess, using Bellman-Ford
    /// from a super-source joined to every surplus node.  found is false when
    /// no surplus node reaches any deficit node (or a negative cycle aborts
    /// the search).  pred_arc holds the predecessor arc of each node in the
    /// residual graph, and dist[n] the shortest distance to node n.
    ///
    /// The residual graph has:
    ///   - Forward arcs (a) with residual = upper_cap - flow, cost = cost
    ///   - Backward arcs (a + num_arcs) with residual = flow, cost = -cost
    struct PathResult {
        bool found = false;
        int src = -1;               ///< surplus node the path starts at
        int dst = -1;               ///< deficit node the path ends at
        int bottleneck = 0;         ///< max flow that can be sent
        std::vector<int> pred_arc;  ///< predecessor arc (-1 = none)
        std::vector<long long> dist;
    };

    [[nodiscard]] static PathResult find_augmenting_path(NetworkData const& data,
                                                         NetworkSolution const& sol);
};

}  // namespace coso
