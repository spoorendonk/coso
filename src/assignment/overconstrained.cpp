#include "assignment/overconstrained.h"

#include <algorithm>
#include <climits>

namespace coso {

// ---------------------------------------------------------------------------
//  Understaffing computation
// ---------------------------------------------------------------------------

int assignment_total_understaffing(
    AssignmentData const& data,
    std::vector<std::vector<int>> const& schedule)
{
    int total = 0;
    int const ne = data.num_employees();
    int const ns = data.num_shift_types();
    int const H  = data.horizon;

    for (int s = 0; s < ns; ++s) {
        for (int d = 0; d < H; ++d) {
            auto dem = data.get_demand(s, d);
            if (dem.min_employees <= 0)
                continue;

            // Count employees assigned to shift s on day d.
            int count = 0;
            for (int e = 0; e < ne; ++e) {
                if (schedule[e][d] == s) {
                    if (dem.required_skill.empty()
                        || std::find(data.employees[e].skills.begin(),
                                     data.employees[e].skills.end(),
                                     dem.required_skill)
                           != data.employees[e].skills.end()) {
                        ++count;
                    }
                }
            }

            if (count < dem.min_employees)
                total += dem.min_employees - count;
        }
    }
    return total;
}

// ---------------------------------------------------------------------------
//  Hard constraint violation counting
// ---------------------------------------------------------------------------

int assignment_total_hard_violations(
    AssignmentData const& data,
    AssignmentCostEvaluator const& eval,
    std::vector<std::vector<int>> const& schedule)
{
    // The cost evaluator computes hard violation cost as
    // count * weight.  We divide by the hard_violation weight
    // to recover the count.  For unavailability, divide by that weight.
    auto const& w = eval.weights();

    int violations = 0;

    if (w.hard_violation > 0) {
        int cost = eval.consecutive_violation_cost(schedule)
                 + eval.rest_violation_cost(schedule)
                 + eval.forbidden_sequence_cost(schedule);
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

int64_t assignment_overconstrained_penalty(
    AssignmentData const& data,
    AssignmentCostEvaluator const& eval,
    std::vector<std::vector<int>> const& schedule,
    AssignmentOverconstrainedConfig const& config)
{
    int64_t penalty = 0;

    // Understaffing penalty.
    penalty += static_cast<int64_t>(assignment_total_understaffing(data, schedule))
               * config.understaffing_penalty;

    // Hard constraint violation penalty.
    penalty += static_cast<int64_t>(
                   assignment_total_hard_violations(data, eval, schedule))
               * config.constraint_violation_penalty;

    return penalty;
}

int64_t assignment_overconstrained_cost(
    AssignmentData const& data,
    AssignmentCostEvaluator const& eval,
    std::vector<std::vector<int>> const& schedule,
    AssignmentOverconstrainedConfig const& config)
{
    // Soft costs that we keep as-is.
    int64_t base = eval.preference_cost(schedule)
                 + eval.replanning_cost(schedule);

    return base + assignment_overconstrained_penalty(
                      data, eval, schedule, config);
}

bool assignment_overconstrained_feasible(
    AssignmentCostEvaluator const& eval,
    std::vector<std::vector<int>> const& schedule)
{
    return eval.is_feasible(schedule);
}

} // namespace coso
