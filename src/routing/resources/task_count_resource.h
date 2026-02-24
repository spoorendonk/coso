#pragma once

#include "routing/problem_data.h"

#include <algorithm>
#include <cassert>

namespace coso {

/// Resource tracking the number of client tasks in a route segment.
///
/// Enforces min_tasks / max_tasks constraints per vehicle type.
/// The state is simply a count of clients in the subsequence.
///
/// Designed for O(1) move evaluation via the standard init/merge/excess
/// resource protocol.
struct TaskCountResource {
    /// State: number of clients in a subsequence.
    struct State {
        int count = 0;
    };

    /// Initialize state for a single client node.
    /// Each client contributes exactly 1 task.
    [[nodiscard]] static State init(
        [[maybe_unused]] ProblemData const& data,
        [[maybe_unused]] int client) {
        assert(client >= 0 && client < data.num_clients());
        return State{1};
    }

    /// Initialize empty state at depot (no tasks).
    [[nodiscard]] static State init_depot(
        [[maybe_unused]] ProblemData const& data) {
        return State{0};
    }

    /// Merge two adjacent subsequence states.
    [[nodiscard]] static State merge(State const& left, State const& right) {
        return State{left.count + right.count};
    }

    /// Merge when the right subsequence is reversed.
    /// Task count is direction-independent, so merge_reverse == merge.
    [[nodiscard]] static State merge_reverse(State const& left,
                                             State const& right) {
        return merge(left, right);
    }

    /// Compute task count excess for a route state against a vehicle type.
    ///
    /// Excess = max(0, min_tasks - count) + max(0, count - max_tasks).
    /// A value of 0 for min_tasks or max_tasks means the constraint is
    /// inactive (no minimum / no maximum).
    ///
    /// @param state         The merged state for the full route.
    /// @param vt            The vehicle type to check constraints against.
    [[nodiscard]] static int excess(State const& state,
                                    ProblemData::VehicleTypeData const& vt) {
        int ex = 0;

        // min_tasks violation: too few clients.
        if (vt.min_tasks > 0 && state.count < vt.min_tasks)
            ex += vt.min_tasks - state.count;

        // max_tasks violation: too many clients.
        if (vt.max_tasks > 0 && state.count > vt.max_tasks)
            ex += state.count - vt.max_tasks;

        return ex;
    }

    /// Convenience: compute excess from explicit min/max values.
    [[nodiscard]] static int excess(State const& state,
                                    int min_tasks, int max_tasks) {
        int ex = 0;
        if (min_tasks > 0 && state.count < min_tasks)
            ex += min_tasks - state.count;
        if (max_tasks > 0 && state.count > max_tasks)
            ex += state.count - max_tasks;
        return ex;
    }
};

} // namespace coso
