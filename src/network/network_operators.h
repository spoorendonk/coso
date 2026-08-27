#pragma once

#include "network/network_solution.h"

#include <vector>

namespace coso {

// ---------------------------------------------------------------------------
//  Move descriptors
// ---------------------------------------------------------------------------

/// Reroute flow along an alternative path between two nodes.
struct RerouteFlowMove {
    int source_node = -1;       ///< start of the reroute segment
    int sink_node = -1;         ///< end of the reroute segment
    int amount = 0;             ///< flow to reroute
    std::vector<int> old_arcs;  ///< arcs losing flow
    std::vector<int> new_arcs;  ///< arcs gaining flow
    long long delta = 0;        ///< cost change (negative = improvement)
};

/// Adjust flow on a single arc within its capacity bounds.
struct AdjustCapacityMove {
    int arc = -1;
    int new_flow = 0;
    long long delta = 0;
};

/// Cancel a negative-cost cycle in the residual graph.
struct CycleCancelMove {
    std::vector<int> cycle_arcs;  ///< arcs forming the cycle (residual graph)
    int amount = 0;               ///< flow to push around the cycle
    long long delta = 0;          ///< cost change (always negative)
};

// ---------------------------------------------------------------------------
//  RerouteFlow operator
// ---------------------------------------------------------------------------

/// Redirect flow along alternative paths to reduce total cost.
///
/// For each arc with positive flow, attempts to find a cheaper alternative
/// path from the arc's tail to its head in the residual graph.
class RerouteFlow {
public:
    /// Find all improving reroute moves.
    [[nodiscard]] static std::vector<RerouteFlowMove> enumerate(NetworkData const& data,
                                                                NetworkSolution const& sol);

    /// Apply a reroute move.
    static void apply(NetworkSolution& sol, RerouteFlowMove const& move);

    /// Evaluate the cost delta of rerouting without applying.
    [[nodiscard]] static long long evaluate(NetworkData const& data, NetworkSolution const& sol,
                                            int arc);
};

// ---------------------------------------------------------------------------
//  AdjustCapacity operator
// ---------------------------------------------------------------------------

/// Modify arc flow within bounds to improve cost, adjusting neighbouring
/// arcs to maintain flow conservation.
class AdjustCapacity {
public:
    /// Find all improving single-arc adjustment moves.
    [[nodiscard]] static std::vector<AdjustCapacityMove> enumerate(NetworkData const& data,
                                                                   NetworkSolution const& sol);

    /// Apply an adjustment move. Note: this may violate flow conservation;
    /// caller is responsible for rebalancing.
    static void apply(NetworkSolution& sol, AdjustCapacityMove const& move);
};

// ---------------------------------------------------------------------------
//  CycleCancel operator
// ---------------------------------------------------------------------------

/// Find and cancel negative-cost cycles in the residual graph.
///
/// This is the classic cycle-cancelling algorithm for MCF optimality:
/// a solution is optimal iff no negative-cost cycles exist in the
/// residual graph.
class CycleCancel {
public:
    /// Find a negative-cost cycle in the residual graph, if one exists.
    /// Returns an empty move (amount == 0) if no negative cycle exists.
    [[nodiscard]] static CycleCancelMove find_negative_cycle(NetworkData const& data,
                                                             NetworkSolution const& sol);

    /// Apply a cycle-cancel move.
    static void apply(NetworkData const& data, NetworkSolution& sol, CycleCancelMove const& move);

    /// Repeatedly cancel negative cycles until none remain.
    /// Returns the number of cycles cancelled.
    static int cancel_all(NetworkData const& data, NetworkSolution& sol);
};

}  // namespace coso
