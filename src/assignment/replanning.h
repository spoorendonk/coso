#pragma once

#include "assignment/assignment_data.h"
#include "assignment/assignment_solution.h"
#include "assignment/cost_evaluator.h"

#include <tuple>
#include <unordered_set>
#include <vector>

namespace coso {

/// Configuration for assignment replanning (re-rostering).
///
/// Allows re-optimizing a roster while keeping some shifts locked (already
/// committed) and incorporating new constraints.  Everything before
/// `horizon_start` is automatically fixed.
struct ReplanConfig {
    /// Locked assignments: (employee, day, shift_type) tuples that must not
    /// change.  These are preserved exactly as-is during replanning.
    std::vector<std::tuple<int, int, int>> locked_assignments;

    /// Additional unavailabilities to apply: (employee, day) pairs.
    std::vector<std::pair<int, int>> new_unavailabilities;

    /// Additional preferences to apply during replanning.
    std::vector<AssignmentData::Preference> new_preferences;

    /// Start day for replanning.  All assignments on days [0, horizon_start)
    /// are automatically locked regardless of `locked_assignments`.
    int horizon_start = 0;
};

/// Tracks which (employee, day) cells are locked during replanning.
///
/// Locked cells cannot be modified by construction or local search operators.
class LockedCells {
public:
    LockedCells() = default;

    /// Build locked cells from a ReplanConfig and the current solution.
    LockedCells(ReplanConfig const& config, AssignmentSolution const& sol);

    /// Check whether a cell is locked.
    [[nodiscard]] bool is_locked(int employee, int day) const
    {
        return locked_.count(key(employee, day)) > 0;
    }

    /// Number of locked cells.
    [[nodiscard]] int size() const noexcept
    {
        return static_cast<int>(locked_.size());
    }

private:
    static int64_t key(int employee, int day) noexcept
    {
        return (static_cast<int64_t>(employee) << 32)
               | static_cast<int64_t>(static_cast<uint32_t>(day));
    }

    std::unordered_set<int64_t> locked_;
};

/// Replan (re-roster) an assignment solution.
///
/// Fixes locked assignments, applies new constraints, clears unlocked cells,
/// reconstructs unassigned slots, and runs local search while respecting
/// locked cells.
///
/// @param sol       The existing solution to replan (modified in place).
/// @param config    Replanning configuration (locked shifts, new constraints).
/// @param data      The assignment instance data (may be modified with new
///                  constraints).
/// @param evaluator The cost evaluator.
void replan(AssignmentSolution& sol,
            ReplanConfig const& config,
            AssignmentData& data,
            AssignmentCostEvaluator const& evaluator);

} // namespace coso
