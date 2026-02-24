#pragma once

#include "routing/problem_data.h"

#include <cassert>

namespace coso {

/// Resource tracking cumulative distance and duration along a route.
///
/// Enforces max_distance and max_duration constraints from the vehicle type.
/// The state at any point in the route contains:
///   - distance: cumulative distance travelled in this subsequence
///   - duration: cumulative duration (travel + service) in this subsequence
///
/// Unlike LoadResource, merging two subsequences requires knowing the travel
/// distance/duration between the last node of the left subsequence and the
/// first node of the right subsequence.  Therefore, merge takes the problem
/// data and the connecting node indices.
///
/// Designed for O(1) move evaluation: merge two adjacent subsequence states
/// without re-scanning clients.
struct DistanceResource {
    /// State for a subsequence: cumulative distance and duration.
    struct State {
        int distance = 0;  ///< Total distance in this subsequence.
        int duration = 0;  ///< Total duration (travel + service) in this subsequence.
        int first    = -1; ///< First node index in this subsequence (-1 = empty/depot).
        int last     = -1; ///< Last node index in this subsequence (-1 = empty/depot).
    };

    /// Initialize state for a single client node.
    ///
    /// A single client has zero distance/duration within its own subsequence
    /// (no travel yet), but contributes its service time to duration.
    ///
    /// @param data     The problem data (for service time lookup).
    /// @param client   Client index (0-based among clients, NOT node index).
    [[nodiscard]] static State init(ProblemData const& data, int client) {
        assert(client >= 0 && client < data.num_clients());
        auto const& c = data.client(client);
        int node = data.num_depots() + client;

        State s;
        s.distance = 0;
        s.duration = c.service;
        s.first    = node;
        s.last     = node;
        return s;
    }

    /// Initialize empty state at depot (no distance/duration).
    ///
    /// @param depot_node  The depot node index (typically 0).
    [[nodiscard]] static State init_depot(int depot_node = 0) {
        State s;
        s.distance = 0;
        s.duration = 0;
        s.first    = depot_node;
        s.last     = depot_node;
        return s;
    }

    /// Merge two adjacent subsequence states (left followed by right).
    ///
    /// Adds the travel distance/duration on the edge connecting the last
    /// node of left to the first node of right.
    ///
    /// @param left     Left subsequence state.
    /// @param right    Right subsequence state.
    /// @param data     Problem data (for distance/duration lookup).
    /// @param profile  Distance profile to use.
    [[nodiscard]] static State merge(State const& left, State const& right,
                                     ProblemData const& data, int profile) {
        State result;

        // Travel from end of left to start of right.
        int edge_dist = data.dist(profile, left.last, right.first);
        int edge_dur  = data.dur(profile, left.last, right.first);

        result.distance = left.distance + edge_dist + right.distance;
        result.duration = left.duration + edge_dur + right.duration;
        result.first    = left.first;
        result.last     = right.last;

        return result;
    }

    /// Compute distance/duration excess for a route state against a vehicle type.
    ///
    /// Returns max(0, distance - max_distance) + max(0, duration - max_duration).
    /// A max_distance or max_duration of 0 means unlimited (no constraint).
    ///
    /// @param state         The merged state for the full route (depot->clients->depot).
    /// @param vehicle_type  The vehicle type to check constraints against.
    [[nodiscard]] static int excess(State const& state,
                                    ProblemData::VehicleTypeData const& vt) {
        int total_excess = 0;

        if (vt.max_distance > 0 && state.distance > vt.max_distance)
            total_excess += state.distance - vt.max_distance;

        if (vt.max_duration > 0 && state.duration > vt.max_duration)
            total_excess += state.duration - vt.max_duration;

        return total_excess;
    }
};

} // namespace coso
