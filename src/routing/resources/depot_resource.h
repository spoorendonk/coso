#pragma once

#include "routing/problem_data.h"

#include <algorithm>
#include <cassert>
#include <climits>

namespace coso {

/// Resource tracking depot assignment for multi-depot VRP.
///
/// Each route starts and ends at a specific depot.  The depot has an
/// open/close time window during which the vehicle must depart from and
/// return to the depot.
///
/// The state tracks:
///   - depot:       Index of the assigned depot (0-based among depots).
///   - depart:      Earliest departure time from the depot.
///   - arrive_back: Earliest time the vehicle returns to the depot
///                  (set after merging the full route with the return-to-depot leg).
///
/// The excess function checks:
///   1. Departure before depot opens => violation (should not happen with
///      correct time-window handling, but is checked for completeness).
///   2. Return after depot closes => excess = arrive_back - depot.tw.end.
///
/// Designed for O(1) move evaluation when the depot assignment is fixed per
/// route.  For multi-depot problems, each route carries its depot index and
/// the optimizer can evaluate reassignment costs.
struct DepotResource {
    struct State {
        int depot = -1;          ///< Depot index (0-based, -1 = unset).
        int depart = 0;          ///< Departure time from depot.
        int arrive_back = 0;     ///< Arrival time back at depot.
        int tw_open = 0;         ///< Depot time window open.
        int tw_close = INT_MAX;  ///< Depot time window close.
    };

    /// Initialize state for a route starting at the given depot.
    ///
    /// @param data   The problem data (for depot time window lookup).
    /// @param depot  Depot index (0-based among depots).
    [[nodiscard]] static State init_depot(ProblemData const& data, int depot) {
        assert(depot >= 0 && depot < data.num_depots());
        auto const& d = data.depot(depot);

        State s;
        s.depot = depot;
        s.depart = d.tw.start;
        s.arrive_back = d.tw.start;  // No clients yet; immediate return.
        s.tw_open = d.tw.start;
        s.tw_close = d.tw.end;
        return s;
    }

    /// Initialize state for a single client node.
    ///
    /// A client node on its own does not have a depot assignment; use
    /// init_depot() for the depot and then merge with client states from
    /// DurationResource.  This method returns an unassigned state with
    /// default times — it exists for interface completeness.
    ///
    /// @param data    The problem data.
    /// @param client  Client index (0-based among clients).
    [[nodiscard]] static State init([[maybe_unused]] ProblemData const& data,
                                    [[maybe_unused]] int client) {
        // Client nodes don't carry depot information themselves.
        State s;
        s.depot = -1;
        s.depart = 0;
        s.arrive_back = 0;
        s.tw_open = 0;
        s.tw_close = INT_MAX;
        return s;
    }

    /// Merge two depot states.
    ///
    /// In typical usage, the left state is the depot start (from init_depot)
    /// and the right state represents the return leg.  The merged state
    /// inherits the depot assignment from whichever side has one, preferring
    /// left (the start depot).
    ///
    /// @param left   Left (earlier) state — usually the depot start.
    /// @param right  Right (later) state — usually the return depot.
    [[nodiscard]] static State merge(State const& left, State const& right) {
        State result;

        // Depot assignment: prefer left (start of route).
        result.depot = (left.depot >= 0) ? left.depot : right.depot;

        // Departure is from the start depot.
        result.depart = (left.depot >= 0) ? left.depart : right.depart;

        // Arrival back is the latest arrival (from the return leg).
        result.arrive_back = std::max(left.arrive_back, right.arrive_back);

        // Time window is the intersection — the depot must be open for
        // both departure and return.
        result.tw_open = (left.depot >= 0) ? left.tw_open : right.tw_open;
        result.tw_close = (left.depot >= 0) ? left.tw_close : right.tw_close;

        return result;
    }

    /// Update the return arrival time.
    ///
    /// Call this after computing the full route duration (including return
    /// travel) to set the time the vehicle arrives back at the depot.
    ///
    /// @param state        The depot state to update.
    /// @param return_time  The time the vehicle arrives back at depot.
    /// @return Updated state with arrive_back set.
    [[nodiscard]] static State with_return_time(State state, int return_time) {
        state.arrive_back = return_time;
        return state;
    }

    /// Compute depot time window excess for a route.
    ///
    /// Violations:
    ///   - Early departure: max(0, tw_open - depart).
    ///   - Late return:     max(0, arrive_back - tw_close).
    ///
    /// @param state  The merged state for the full route.
    [[nodiscard]] static int excess(State const& state) {
        int total = 0;

        // Early departure violation (depart before depot opens).
        if (state.depart < state.tw_open) {
            total += state.tw_open - state.depart;
        }

        // Late return violation (arrive after depot closes).
        if (state.arrive_back > state.tw_close) {
            total += state.arrive_back - state.tw_close;
        }

        return total;
    }

    /// Compute depot excess using vehicle type data (for interface consistency).
    ///
    /// The vehicle type itself doesn't constrain depot time windows — the
    /// depot does.  This overload ignores the vehicle type and delegates to
    /// the simpler excess(State).
    [[nodiscard]] static int excess(State const& state,
                                    [[maybe_unused]] ProblemData::VehicleTypeData const& vt) {
        return excess(state);
    }

    /// Check whether a depot is assigned.
    [[nodiscard]] static bool has_depot(State const& state) noexcept { return state.depot >= 0; }

    /// Check whether the route departs and returns within the depot TW.
    [[nodiscard]] static bool is_feasible(State const& state) noexcept {
        return excess(state) == 0;
    }
};

}  // namespace coso
