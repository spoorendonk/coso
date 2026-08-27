#pragma once

#include "scheduling/schedule_data.h"

#include <cassert>
#include <vector>

namespace coso {

/// Represents a mode for an operation in multi-mode RCPSP.
///
/// Each mode defines an alternative way to execute an operation, with its own
/// duration and resource usage profile.
struct OperationMode {
    int duration = 0;                 ///< processing time in this mode
    std::vector<int> resource_usage;  ///< usage per resource (indexed by resource id)
};

/// Stores operation -> mode mapping for multi-mode RCPSP.
///
/// Each operation may have multiple execution modes (different duration and
/// resource consumption trade-offs). A ModeAssignment selects exactly one
/// mode per operation.
class ModeAssignment {
public:
    /// Construct a mode assignment for the given problem.
    /// Initialises all operations to mode 0 (first available mode).
    explicit ModeAssignment(ScheduleData const& data);

    /// Number of operations.
    [[nodiscard]] int num_operations() const noexcept {
        return static_cast<int>(selected_mode_.size());
    }

    /// Number of modes available for operation op.
    [[nodiscard]] int num_modes(int op) const {
        assert(op >= 0 && op < static_cast<int>(modes_.size()));
        return static_cast<int>(modes_[op].size());
    }

    /// Get the currently selected mode index for an operation.
    [[nodiscard]] int selected_mode(int op) const {
        assert(op >= 0 && op < static_cast<int>(selected_mode_.size()));
        return selected_mode_[op];
    }

    /// Set the selected mode for an operation.
    void set_mode(int op, int mode) {
        assert(op >= 0 && op < static_cast<int>(selected_mode_.size()));
        assert(mode >= 0 && mode < num_modes(op));
        selected_mode_[op] = mode;
    }

    /// Get the mode data for operation op, mode index m.
    [[nodiscard]] OperationMode const& mode(int op, int m) const {
        assert(op >= 0 && op < static_cast<int>(modes_.size()));
        assert(m >= 0 && m < static_cast<int>(modes_[op].size()));
        return modes_[op][m];
    }

    /// Get the currently active mode for an operation.
    [[nodiscard]] OperationMode const& active_mode(int op) const {
        return mode(op, selected_mode(op));
    }

    /// Duration of the currently selected mode for an operation.
    [[nodiscard]] int duration(int op) const { return active_mode(op).duration; }

    /// Resource usage of the currently selected mode for operation op,
    /// resource r.
    [[nodiscard]] int resource_usage(int op, int r) const {
        auto const& m = active_mode(op);
        if (r < static_cast<int>(m.resource_usage.size())) {
            return m.resource_usage[r];
        }
        return 0;
    }

    /// Total resource usage across all operations for resource r under
    /// the current mode assignment.
    [[nodiscard]] int total_resource_usage(int r) const;

    /// Total weighted cost: sum over all operations of duration (can be
    /// extended to a weighted objective).
    [[nodiscard]] int total_duration() const;

    /// Check resource feasibility: for every time step, the sum of resource
    /// usages of concurrently executing operations does not exceed capacity.
    /// This is a simplified check assuming all operations run sequentially
    /// from t=0 (pessimistic). For true feasibility, a schedule is needed.
    /// Here we just check per-operation: each operation's resource usage
    /// in its selected mode does not exceed the resource capacity.
    [[nodiscard]] bool mode_resource_feasible(ScheduleData const& data) const;

    /// Add a mode for an operation (used during construction).
    void add_mode(int op, OperationMode m);

    /// Access all modes for an operation.
    [[nodiscard]] std::vector<OperationMode> const& modes(int op) const {
        assert(op >= 0 && op < static_cast<int>(modes_.size()));
        return modes_[op];
    }

private:
    std::vector<std::vector<OperationMode>> modes_;  ///< modes_[op] = list of modes
    std::vector<int> selected_mode_;                 ///< selected mode per operation
};

/// Greedy mode selection: for each operation, pick the mode that minimises
/// total resource usage while keeping per-resource usage within capacity.
/// Falls back to the shortest-duration mode if all modes are capacity-feasible.
[[nodiscard]] ModeAssignment greedy_mode_selection(ScheduleData const& data);

/// Local search improvement over mode assignments.
///
/// Tries single-operation mode swaps and accepts any swap that reduces total
/// duration without violating per-operation resource capacity constraints.
/// Runs until no improving swap is found (steepest descent).
void local_search_modes(ScheduleData const& data, ModeAssignment& assignment);

}  // namespace coso
