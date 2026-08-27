#pragma once

#include "routing/problem_data.h"

#include <algorithm>
#include <cassert>
#include <climits>

namespace coso {

/// Resource tracking time/duration along a route with time window feasibility.
///
/// Uses the forward-label concatenation scheme from Vidal et al. (2014).
/// For a subsequence (i_1, ..., i_k) of clients, the state tracks:
///
///   - earliest:   Earliest time we can *depart* from the last node i_k,
///                 assuming we depart from the predecessor of i_1 at time 0.
///   - latest:     Latest time we can *arrive* at the first node i_1 without
///                 causing any additional time warp beyond what is already
///                 recorded in `time_warp`.
///   - duration:   Total travel + service time through the subsequence
///                 (excluding waiting, but including internal travel times).
///   - time_warp:  Total time window violation accumulated within.
///   - first_node: Node index of the first client in this subsequence.
///   - last_node:  Node index of the last client in this subsequence.
///
/// The `first_node` / `last_node` are needed so that merge() can look up
/// travel times between the junction nodes (last of left, first of right).
///
/// A feasible route has time_warp == 0.
struct DurationResource {
    struct State {
        int earliest = 0;      ///< Earliest departure from last node.
        int latest = INT_MAX;  ///< Latest feasible arrival at first node.
        int duration = 0;      ///< Travel + service (no wait) through subseq.
        int time_warp = 0;     ///< Total TW violation in this subsequence.
        int first_node = -1;   ///< Node index of first client (full numbering).
        int last_node = -1;    ///< Node index of last client (full numbering).
    };

    /// Initialize state for a single client node.
    ///
    /// @param data     The problem data.
    /// @param client   Client index (0-based among clients).
    /// @param node     Full node index (num_depots + client).
    [[nodiscard]] static State init(ProblemData const& data, int client, int node) {
        assert(client >= 0 && client < data.num_clients());
        auto const& c = data.client(client);

        State s;
        s.earliest = c.tw.start + c.service;  // earliest departure
        s.latest = c.tw.end;                  // latest arrival without warp
        s.duration = c.service;
        s.time_warp = 0;
        s.first_node = node;
        s.last_node = node;

        return s;
    }

    /// Initialize state at depot.
    [[nodiscard]] static State init_depot(ProblemData const& data, int depot_node = 0) {
        assert(depot_node >= 0 && depot_node < data.num_depots());
        auto const& d = data.depot(depot_node);

        State s;
        s.earliest = d.tw.start;  // earliest departure from depot
        s.latest = d.tw.end;      // latest departure from depot
        s.duration = 0;
        s.time_warp = 0;
        s.first_node = depot_node;
        s.last_node = depot_node;

        return s;
    }

    /// Initialize empty state (identity element for merge).
    /// Used as "no clients" sentinel in prefix/suffix arrays.
    [[nodiscard]] static State init_empty() {
        State s;
        s.earliest = 0;
        s.latest = INT_MAX;
        s.duration = 0;
        s.time_warp = 0;
        s.first_node = -1;
        s.last_node = -1;
        return s;
    }

    /// Merge two adjacent subsequences with explicit travel time between them.
    ///
    /// @param left         State of the left (earlier) subsequence.
    /// @param right        State of the right (later) subsequence.
    /// @param travel_time  Duration from left's last node to right's first node.
    /// @return Merged state for the combined subsequence.
    ///
    /// Formulation (Vidal et al. 2014):
    ///   arrival_at_right = E_left + travel_time
    ///   delta_warp = max(0, arrival_at_right - L_right)
    ///
    ///   E_merged = max(arrival_at_right, E_right - D_right) + D_right
    ///   L_merged = min(L_left, L_right - D_left - travel_time + W_left)
    ///   D_merged = D_left + travel_time + D_right
    ///   W_merged = W_left + W_right + delta_warp
    [[nodiscard]] static State merge(State const& left, State const& right, int travel_time) {
        State result;

        int arrival = left.earliest + travel_time;

        // Time warp at the junction: arrive after right's latest feasible.
        int warp = std::max(0, arrival - right.latest);

        // The "effective TW start" of right's first node.
        // right.earliest - right.duration is the earliest arrival at right's
        // first node such that everything in right can proceed on time.
        int right_tw_start = right.earliest - right.duration;

        // Earliest departure from the combined subsequence's last node.
        result.earliest = std::max(arrival, right_tw_start) + right.duration;

        // Latest feasible arrival at the combined subsequence's first node.
        // Pushing left's start later by delta shifts arrival at right by delta.
        // The +W_left accounts for time warp already in left that could be
        // "recovered" by arriving later.
        result.latest =
            std::min(left.latest, right.latest - travel_time - left.duration + left.time_warp);

        result.duration = left.duration + travel_time + right.duration;
        result.time_warp = left.time_warp + right.time_warp + warp;

        result.first_node = left.first_node;
        result.last_node = right.last_node;

        return result;
    }

    /// Merge using the problem data's duration matrix.
    ///
    /// Looks up travel time from left.last_node to right.first_node.
    [[nodiscard]] static State merge(State const& left, State const& right, ProblemData const& data,
                                     int profile) {
        assert(left.last_node >= 0 && right.first_node >= 0);
        int travel = data.dur(profile, left.last_node, right.first_node);
        return merge(left, right, travel);
    }

    /// Compute time window excess for the route state.
    ///
    /// Returns total time warp. Also adds max_duration violation if the
    /// vehicle type has a max_duration constraint.
    [[nodiscard]] static int excess(State const& state, ProblemData::VehicleTypeData const& vt) {
        int total = state.time_warp;

        if (vt.max_duration > 0 && state.earliest > vt.max_duration) {
            total += state.earliest - vt.max_duration;
        }

        return total;
    }

    /// Convenience: just the time warp (no max_duration check).
    [[nodiscard]] static int time_warp(State const& state) { return state.time_warp; }
};

}  // namespace coso
