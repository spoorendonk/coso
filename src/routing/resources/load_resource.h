#pragma once

#include "routing/problem_data.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace coso {

/// Resource tracking cumulative load along a route.
///
/// Supports multi-dimensional loads (weight, volume, etc.).  Each dimension
/// is tracked independently.  The state at any point in the route contains:
///   - delivery: total demand delivered so far (cumulative from start)
///   - pickup:   total pickup collected so far (cumulative from start)
///   - load:     maximum vehicle load observed in this subsequence
///
/// The excess function reports how much the maximum load exceeds the vehicle
/// capacity for each dimension (summed across dimensions).
///
/// Designed for O(1) move evaluation: merge two adjacent subsequence states
/// without re-scanning clients.
struct LoadResource {
    /// State for a single load dimension within a subsequence.
    struct DimState {
        int delivery = 0;  ///< Total delivery demand in this subsequence.
        int pickup = 0;    ///< Total pickup quantity in this subsequence.
        int load = 0;      ///< Maximum load observed in this subsequence.
    };

    /// Multi-dimensional state: one DimState per load dimension.
    struct State {
        std::vector<DimState> dims;

        /// Number of load dimensions.
        [[nodiscard]] int num_dims() const noexcept { return static_cast<int>(dims.size()); }
    };

    /// Initialize state for a single client node.
    ///
    /// @param data     The problem data (for client demand/pickup lookup).
    /// @param client   Client index (0-based among clients, NOT node index).
    [[nodiscard]] static State init(ProblemData const& data, int client) {
        assert(client >= 0 && client < data.num_clients());
        auto const& c = data.client(client);
        int nd = data.num_load_dims();

        State s;
        s.dims.resize(nd);
        for (int d = 0; d < nd; ++d) {
            s.dims[d].delivery = c.demand[d];
            s.dims[d].pickup = c.pickup[d];
            s.dims[d].load = c.demand[d] + c.pickup[d];
        }
        return s;
    }

    /// Initialize empty state at depot (no load).
    [[nodiscard]] static State init_depot(ProblemData const& data) {
        int nd = data.num_load_dims();
        State s;
        s.dims.resize(nd, DimState{0, 0, 0});
        return s;
    }

    /// Merge two adjacent subsequence states (left followed by right).
    ///
    /// After merging, the resulting state represents the combined subsequence.
    /// The vehicle starts with all delivery demand loaded, delivers along the
    /// route, and picks up along the way.
    ///
    /// Load at any point = (total_delivery - delivered_so_far) + picked_up_so_far
    /// The maximum load in the combined subsequence is the max of:
    ///   - max load in the left subsequence
    ///   - max load in the right subsequence, adjusted for left's deliveries/pickups
    [[nodiscard]] static State merge(State const& left, State const& right) {
        int nd = left.num_dims();
        assert(nd == right.num_dims());

        State result;
        result.dims.resize(nd);

        for (int d = 0; d < nd; ++d) {
            auto const& l = left.dims[d];
            auto const& r = right.dims[d];

            result.dims[d].delivery = l.delivery + r.delivery;
            result.dims[d].pickup = l.pickup + r.pickup;

            // Load at any point in the combined route:
            // In the left part: load_left(i) = (total_delivery - delivered_left(i)) +
            // picked_up_left(i)
            //   but we need to account for right's delivery too, since all delivery
            //   is loaded at start: load_left(i) += right.delivery
            // In the right part: load_right(j) adjusted by left's net effect
            //   load_right(j) = (right.delivery - delivered_right(j)) + picked_up_right(j) +
            //   left.pickup but right.load already captures max of (right.delivery - del + pick)
            //   within right
            //
            // Max load in left part = left.load + right.delivery (additional delivery loaded)
            // Max load in right part = right.load + left.pickup (additional pickup accumulated)
            int max_left = l.load + r.delivery;
            int max_right = r.load + l.pickup;
            result.dims[d].load = std::max(max_left, max_right);
        }

        return result;
    }

    /// Merge when the right subsequence is reversed.
    ///
    /// For LoadResource, the load is direction-independent: reversing a
    /// subsequence doesn't change the maximum load within it (the set of
    /// clients visited is the same, just in reverse order).  However, the
    /// delivery/pickup pattern differs when reversed.
    ///
    /// When reversed, what was "delivery" in the original direction becomes
    /// effectively traversed in the opposite order.  But since we track totals
    /// and max load, and the load at any point is determined by the cumulative
    /// delivered/picked quantities, reversing doesn't change the max load
    /// within the subsequence itself.
    ///
    /// Thus merge_reverse == merge for LoadResource.
    [[nodiscard]] static State merge_reverse(State const& left, State const& right) {
        return merge(left, right);
    }

    /// Compute capacity excess for a route state against a vehicle type.
    ///
    /// Returns the total excess across all load dimensions (sum of
    /// max(0, load - capacity) per dimension).
    ///
    /// @param state         The merged state for the full route.
    /// @param vehicle_type  The vehicle type to check capacity against.
    [[nodiscard]] static int excess(State const& state, ProblemData::VehicleTypeData const& vt) {
        int total_excess = 0;
        int nd = state.num_dims();
        for (int d = 0; d < nd; ++d) {
            int cap = (d < static_cast<int>(vt.capacity.size())) ? vt.capacity[d] : 0;
            if (state.dims[d].load > cap) {
                total_excess += state.dims[d].load - cap;
            }
        }
        return total_excess;
    }

    /// Convenience: compute excess from capacity vector directly.
    [[nodiscard]] static int excess(State const& state, std::vector<int> const& capacity) {
        int total_excess = 0;
        int nd = state.num_dims();
        for (int d = 0; d < nd; ++d) {
            int cap = (d < static_cast<int>(capacity.size())) ? capacity[d] : 0;
            if (state.dims[d].load > cap) {
                total_excess += state.dims[d].load - cap;
            }
        }
        return total_excess;
    }
};

}  // namespace coso
