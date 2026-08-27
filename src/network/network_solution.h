#pragma once

#include "network/network_data.h"

#include <cassert>
#include <vector>

namespace coso {

/// Mutable network flow solution: tracks flow on each arc.
///
/// Provides objective evaluation (total cost) and feasibility checking
/// (flow conservation, capacity bounds, resource constraints).
class NetworkSolution {
public:
    /// Construct a zero-flow solution for the given network.
    explicit NetworkSolution(NetworkData const& data);

    // -------------------------------------------------------------------
    //  Accessors
    // -------------------------------------------------------------------

    /// The network data this solution belongs to.
    [[nodiscard]] NetworkData const& data() const noexcept { return data_; }

    /// Flow on arc a.
    [[nodiscard]] int flow(int a) const {
        assert(a >= 0 && a < static_cast<int>(flow_.size()));
        return flow_[a];
    }

    /// Residual capacity of arc a (upper_cap - flow).
    [[nodiscard]] int residual(int a) const {
        auto const& ad = data_.arc(a);
        return ad.upper_cap - flow_[a];
    }

    /// Net excess at node n: supply + inflow - outflow.
    /// A feasible solution has excess == 0 at every node.
    [[nodiscard]] int excess(int n) const {
        assert(n >= 0 && n < data_.num_nodes());
        return excess_[n];
    }

    // -------------------------------------------------------------------
    //  Modification
    // -------------------------------------------------------------------

    /// Set flow on arc a.
    void set_flow(int a, int amount);

    /// Add delta to flow on arc a.
    void add_flow(int a, int delta);

    // -------------------------------------------------------------------
    //  Objective
    // -------------------------------------------------------------------

    /// Total cost: sum over all arcs of flow[a] * cost[a].
    [[nodiscard]] long long cost() const noexcept { return cost_; }

    // -------------------------------------------------------------------
    //  Feasibility
    // -------------------------------------------------------------------

    /// Overall feasibility: conservation, capacity, and resource constraints.
    [[nodiscard]] bool feasible() const;

    /// Check flow conservation: excess == 0 at every node.
    [[nodiscard]] bool flow_conservation() const;

    /// Check capacity bounds: lower_cap <= flow <= upper_cap for all arcs.
    [[nodiscard]] bool capacity_feasible() const;

    /// Check resource constraints (if any). For each resource, the total
    /// consumption (sum of flow[a] * usage[a][r]) must not exceed the bound.
    [[nodiscard]] bool resource_feasible() const;

    /// Number of nodes with non-zero excess.
    [[nodiscard]] int num_excess_violations() const;

    /// Number of arcs violating capacity bounds.
    [[nodiscard]] int num_capacity_violations() const;

private:
    NetworkData const& data_;
    std::vector<int> flow_;
    std::vector<int> excess_;  ///< net excess at each node
    long long cost_ = 0;

    /// Recompute excess_ from scratch.
    void recompute_excess_();
};

}  // namespace coso
