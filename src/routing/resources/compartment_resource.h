#pragma once

#include <algorithm>
#include <cassert>
#include <vector>

namespace coso {

/// Resource tracking multi-compartment vehicle loading.
///
/// Each vehicle has multiple compartments (e.g., frozen/fresh/ambient), each
/// with its own capacity.  Each client's demand is assigned to a specific
/// compartment.  The state tracks per-compartment cumulative load, and the
/// excess function reports total over-capacity across all compartments.
///
/// Compartment assignment is specified per-client via an external lookup
/// table (CompartmentInfo).  Clients not assigned to any compartment are
/// ignored by this resource.
///
/// Designed for O(1) move evaluation: merge two adjacent subsequence states
/// via element-wise addition.
struct CompartmentResource {
    /// Per-client compartment assignment.
    struct ClientInfo {
        int compartment = -1;  ///< Compartment index, or -1 if not assigned.
        int demand = 0;        ///< Demand for that compartment.
    };

    /// State: per-compartment load in a subsequence.
    struct State {
        std::vector<int> loads;  ///< Load per compartment.

        /// Number of compartments.
        [[nodiscard]] int num_compartments() const noexcept {
            return static_cast<int>(loads.size());
        }
    };

    /// Initialize state for a single client node.
    ///
    /// @param info       Per-client compartment info lookup.
    /// @param client     Client index (0-based among clients).
    /// @param num_comps  Total number of compartments.
    [[nodiscard]] static State init(std::vector<ClientInfo> const& info, int client,
                                    int num_comps) {
        assert(client >= 0 && client < static_cast<int>(info.size()));
        State s;
        s.loads.resize(num_comps, 0);

        auto const& ci = info[client];
        if (ci.compartment >= 0 && ci.compartment < num_comps) {
            s.loads[ci.compartment] = ci.demand;
        }

        return s;
    }

    /// Initialize empty state at depot (no load in any compartment).
    [[nodiscard]] static State init_depot(int num_comps) {
        State s;
        s.loads.resize(num_comps, 0);
        return s;
    }

    /// Merge two adjacent subsequence states.
    ///
    /// Per-compartment loads are summed.
    [[nodiscard]] static State merge(State const& left, State const& right) {
        int nc = left.num_compartments();
        assert(nc == right.num_compartments());

        State result;
        result.loads.resize(nc);

        for (int c = 0; c < nc; ++c) {
            result.loads[c] = left.loads[c] + right.loads[c];
        }

        return result;
    }

    /// Merge when the right subsequence is reversed.
    ///
    /// Compartment loads are order-independent, so merge_reverse == merge.
    [[nodiscard]] static State merge_reverse(State const& left, State const& right) {
        return merge(left, right);
    }

    /// Compute compartment capacity excess.
    ///
    /// Returns the sum of max(0, load - capacity) across all compartments.
    ///
    /// @param state       The merged state for the full route.
    /// @param capacities  Per-compartment capacities.
    [[nodiscard]] static int excess(State const& state, std::vector<int> const& capacities) {
        int total = 0;
        int nc = state.num_compartments();

        for (int c = 0; c < nc; ++c) {
            int cap = (c < static_cast<int>(capacities.size())) ? capacities[c] : 0;
            if (state.loads[c] > cap) {
                total += state.loads[c] - cap;
            }
        }

        return total;
    }
};

}  // namespace coso
