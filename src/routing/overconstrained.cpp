#include "routing/overconstrained.h"

namespace coso {

// ---------------------------------------------------------------------------
//  Aggregate violation helpers
// ---------------------------------------------------------------------------

int num_unserved_required(Solution const& sol) {
    int count = 0;
    for (int c : sol.unassigned()) {
        if (sol.data().client(c).required) {
            ++count;
        }
    }
    return count;
}

int total_load_excess(Solution const& sol) {
    int total = 0;
    for (auto const& r : sol.routes()) {
        total += r.load_excess();
    }
    return total;
}

Measure total_time_warp(Solution const& sol) {
    Measure total = 0;
    for (auto const& r : sol.routes()) {
        total += r.time_warp();
    }
    return total;
}

// ---------------------------------------------------------------------------
//  Overconstrained cost evaluation
// ---------------------------------------------------------------------------

Cost overconstrained_penalty(Solution const& sol, OverconstrainedConfig const& config) {
    Cost penalty = 0;

    // Unserved client penalties.
    if (config.allow_unserved) {
        penalty += static_cast<Cost>(num_unserved_required(sol)) * config.unserved_penalty;
    }

    // Capacity violation penalties.
    penalty += static_cast<Cost>(total_load_excess(sol)) * config.capacity_violation_penalty;

    // Time window violation penalties.
    penalty += static_cast<Cost>(total_time_warp(sol)) * config.tw_violation_penalty;

    return penalty;
}

Cost overconstrained_cost(Solution const& sol, OverconstrainedConfig const& config) {
    // Base objective: distance + fixed costs - prizes, no search penalties.
    CostEvaluator zero_eval(0, 0, 0);
    Cost obj = sol.objective(zero_eval);

    return obj + overconstrained_penalty(sol, config);
}

bool overconstrained_feasible(Solution const& sol, OverconstrainedConfig const& config) {
    // Check unserved required clients.
    if (!config.allow_unserved && num_unserved_required(sol) > 0) {
        return false;
    }

    // Check route-level feasibility.
    for (auto const& r : sol.routes()) {
        if (r.load_excess() > 0) {
            return false;
        }
        if (!r.tw_feasible()) {
            return false;
        }
    }

    return true;
}

}  // namespace coso
