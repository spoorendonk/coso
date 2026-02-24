#pragma once

#include "assignment/assignment_data.h"
#include "assignment/cost_evaluator.h"

#include <cstdint>
#include <vector>

namespace coso {

/// Configuration for overconstrained assignment (nurse rostering) instances.
///
/// When a rostering instance has too few employees to meet all demand, or
/// hard constraints conflict with demand, the solver can relax constraints
/// with penalty costs to find a "least bad" schedule.
struct AssignmentOverconstrainedConfig {
    int64_t understaffing_penalty       = 10000; ///< Cost per missing shift (unmet demand).
    int64_t constraint_violation_penalty = 5000;  ///< Cost per soft constraint violation.
};

/// Compute the overconstrained penalty for an assignment schedule.
///
/// Penalties:
///   - understaffing_penalty * total_understaffing (shifts below min demand)
///   - constraint_violation_penalty * total_hard_constraint_violations
///
/// @param data      The assignment instance data.
/// @param eval      The cost evaluator (provides component cost functions).
/// @param schedule  The schedule matrix: schedule[employee][day] = shift_type.
/// @param config    Overconstrained configuration.
/// @return The overconstrained penalty cost (non-negative).
[[nodiscard]] int64_t assignment_overconstrained_penalty(
    AssignmentData const& data,
    AssignmentCostEvaluator const& eval,
    std::vector<std::vector<int>> const& schedule,
    AssignmentOverconstrainedConfig const& config);

/// Compute the total overconstrained cost for an assignment schedule.
///
/// Total = preference_cost + replanning_cost + overconstrained_penalty.
/// This excludes the regular demand and hard constraint costs from the
/// evaluator (they are replaced by the overconstrained penalties).
///
/// @param data      The assignment instance data.
/// @param eval      The cost evaluator.
/// @param schedule  The schedule matrix.
/// @param config    Overconstrained configuration.
/// @return Total overconstrained cost.
[[nodiscard]] int64_t assignment_overconstrained_cost(
    AssignmentData const& data,
    AssignmentCostEvaluator const& eval,
    std::vector<std::vector<int>> const& schedule,
    AssignmentOverconstrainedConfig const& config);

/// Count total understaffing across all shift types and days.
///
/// For each (shift_type, day), if the count of assigned employees is below
/// the minimum demand, the shortfall is accumulated.
///
/// @param data      The assignment instance data.
/// @param schedule  The schedule matrix.
/// @return Total number of missing employee-shifts.
[[nodiscard]] int assignment_total_understaffing(
    AssignmentData const& data,
    std::vector<std::vector<int>> const& schedule);

/// Count total hard constraint violations.
///
/// Includes consecutive shift violations, rest violations, forbidden
/// sequence violations, and unavailability violations.
///
/// @param data      The assignment instance data.
/// @param eval      The cost evaluator (provides component functions).
/// @param schedule  The schedule matrix.
/// @return Total number of hard constraint violations (weighted by eval).
[[nodiscard]] int assignment_total_hard_violations(
    AssignmentData const& data,
    AssignmentCostEvaluator const& eval,
    std::vector<std::vector<int>> const& schedule);

/// Check whether a schedule is overconstrained-feasible.
///
/// In overconstrained mode, a schedule is always "feasible" in the sense
/// that we accept understaffing and constraint violations (they are just
/// penalized).  This function checks whether the actual hard constraints
/// are satisfied (no violations at all).
///
/// @param eval      The cost evaluator.
/// @param schedule  The schedule matrix.
/// @return true if no hard constraint violations exist.
[[nodiscard]] bool assignment_overconstrained_feasible(
    AssignmentCostEvaluator const& eval,
    std::vector<std::vector<int>> const& schedule);

} // namespace coso
