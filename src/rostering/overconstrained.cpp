#include "rostering/overconstrained.h"

#include "rostering/cover.h"

#include <algorithm>
#include <climits>

namespace coso {

// ---------------------------------------------------------------------------
//  Understaffing computation
// ---------------------------------------------------------------------------

int rostering_total_understaffing(RosteringData const& data,
                                  std::vector<std::vector<int>> const& schedule) {
    int total = 0;
    int const ns = data.num_shift_types();
    int const H = data.horizon;

    for (int s = 0; s < ns; ++s) {
        for (int d = 0; d < H; ++d) {
            auto const demands = data.get_demands(s, d);
            if (demands.empty()) {
                continue;
            }
            // Workers credited to the cell's demands as rostering/cover.h
            // describes.
            auto const counts = credit_cover(data, demands, cell_workers(schedule, s, d));
            for (size_t j = 0; j < demands.size(); ++j) {
                total += cover_units(demands[j], counts[j]).first;
            }
        }
    }
    return total;
}

// ---------------------------------------------------------------------------
//  Hard constraint violation counting
// ---------------------------------------------------------------------------

int rostering_total_hard_violations(RosteringData const& data, RosteringCostEvaluator const& eval,
                                    std::vector<std::vector<int>> const& schedule) {
    // The cost evaluator computes hard violation cost as
    // count * weight.  We divide by the hard_violation weight
    // to recover the count.  For unavailability, divide by that weight.
    auto const& w = eval.weights();

    int violations = 0;

    if (w.hard_violation > 0) {
        int cost =
            eval.consecutive_violation_cost(schedule) + eval.forbidden_sequence_cost(schedule);
        violations += cost / w.hard_violation;
    }

    if (w.unavailability > 0) {
        violations += eval.unavailability_cost(schedule) / w.unavailability;
    }

    return violations;
}

// ---------------------------------------------------------------------------
//  Overconstrained cost evaluation
// ---------------------------------------------------------------------------

int64_t rostering_overconstrained_penalty(RosteringData const& data,
                                          RosteringCostEvaluator const& eval,
                                          std::vector<std::vector<int>> const& schedule,
                                          RosteringOverconstrainedConfig const& config) {
    int64_t penalty = 0;

    // Understaffing penalty.
    penalty += static_cast<int64_t>(rostering_total_understaffing(data, schedule)) *
               config.understaffing_penalty;

    // Hard constraint violation penalty.
    penalty += static_cast<int64_t>(rostering_total_hard_violations(data, eval, schedule)) *
               config.constraint_violation_penalty;

    return penalty;
}

int64_t rostering_overconstrained_cost(RosteringData const& data,
                                       RosteringCostEvaluator const& eval,
                                       std::vector<std::vector<int>> const& schedule,
                                       RosteringOverconstrainedConfig const& config) {
    // Soft costs that we keep as-is.
    int64_t base = eval.preference_cost(schedule);

    return base + rostering_overconstrained_penalty(data, eval, schedule, config);
}

bool rostering_overconstrained_feasible(RosteringCostEvaluator const& eval,
                                        std::vector<std::vector<int>> const& schedule) {
    return eval.is_feasible(schedule);
}

}  // namespace coso
