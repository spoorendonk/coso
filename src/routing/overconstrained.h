#pragma once

#include "routing/solution.h"

#include <cstdint>

namespace coso {

/// Configuration for overconstrained routing instances.
///
/// When an instance has no feasible solution (e.g., total demand exceeds fleet
/// capacity, or time windows are too tight), the solver can still find a
/// "least bad" solution by treating hard constraints as soft with penalty
/// costs.  This struct controls which relaxations are allowed and their
/// penalty weights.
struct OverconstrainedConfig {
    bool allow_unserved = false;                ///< Allow clients to be left unserved.
    int64_t unserved_penalty = 10000;           ///< Cost per unserved client.
    int64_t tw_violation_penalty = 1000;        ///< Cost per unit of time window violation.
    int64_t capacity_violation_penalty = 1000;  ///< Cost per unit of capacity excess.
};

/// Compute the overconstrained penalty cost for a routing solution.
///
/// This adds to the regular solution cost:
///   - unserved_penalty * num_unserved  (if allow_unserved is true)
///   - tw_violation_penalty * total_time_warp
///   - capacity_violation_penalty * total_load_excess
///
/// @param sol     The routing solution to evaluate.
/// @param config  Overconstrained configuration with penalty weights.
/// @return The total overconstrained penalty cost (non-negative).
[[nodiscard]] int64_t overconstrained_penalty(Solution const& sol,
                                              OverconstrainedConfig const& config);

/// Compute the total overconstrained cost: regular objective + overconstrained
/// penalties.
///
/// This is intended for final solution evaluation in overconstrained mode.
/// It uses a zero-penalty CostEvaluator for the base objective (distance +
/// fixed costs - prizes) and adds overconstrained penalties on top.
///
/// @param sol     The routing solution to evaluate.
/// @param config  Overconstrained configuration with penalty weights.
/// @return Total overconstrained cost = objective + overconstrained penalties.
[[nodiscard]] int64_t overconstrained_cost(Solution const& sol,
                                           OverconstrainedConfig const& config);

/// Compute the number of unserved required clients.
///
/// Optional clients (required == false) are not counted as unserved
/// violations; they simply lose their prize credit in the objective.
///
/// @param sol  The routing solution.
/// @return Number of unserved clients that are marked as required.
[[nodiscard]] int num_unserved_required(Solution const& sol);

/// Compute the total load excess across all routes.
///
/// @param sol  The routing solution.
/// @return Sum of load_excess() across all routes.
[[nodiscard]] int total_load_excess(Solution const& sol);

/// Compute the total time warp across all routes.
///
/// @param sol  The routing solution.
/// @return Sum of time_warp() across all routes.
[[nodiscard]] int total_time_warp(Solution const& sol);

/// Check whether a solution is overconstrained-feasible.
///
/// A solution is overconstrained-feasible if:
///   - No required clients are unserved (unless allow_unserved is true).
///   - All routes are load-feasible and TW-feasible.
///
/// In overconstrained mode with allow_unserved, unserved clients are
/// acceptable (but penalized).
///
/// @param sol     The routing solution.
/// @param config  Overconstrained configuration.
/// @return true if the solution satisfies overconstrained feasibility.
[[nodiscard]] bool overconstrained_feasible(Solution const& sol,
                                            OverconstrainedConfig const& config);

}  // namespace coso
