#pragma once

#include "rostering/cost_evaluator.h"
#include "rostering/rostering_data.h"
#include "rostering/rostering_solution.h"

namespace coso {

// Both constructions never make an assignment that breaks unavailability,
// max_consecutive_days, a forbidden sequence, max_weekends or
// max_total_minutes, and finish by topping each employee up to
// min_total_minutes.  An employee they cannot top up is left short, so the
// returned roster is_feasible() == false rather than silently wrong.  The
// top-up is greedy, so a minimum it misses is not proof none is reachable.

/// Whether assigning employee e to shift s on day d keeps every hard bound
/// a single assignment can break: unavailability, a free day,
/// max_consecutive_days, forbidden sequences, max_weekends and
/// max_total_minutes.  min_total_minutes is a lower bound no assignment
/// breaks, so it is not checked here.
[[nodiscard]] bool is_feasible_assignment(RosteringData const& data,
                                          std::vector<std::vector<int>> const& schedule, int e,
                                          int d, int s);

/// First-Fit-Decreasing construction for nurse rostering.
///
/// Sorts demands by required employees (descending) and greedily assigns the
/// cheapest available employee to each unmet demand slot.
RosteringSolution construct_ffd(RosteringData const& data, RosteringCostEvaluator const& evaluator);

/// Greedy day-by-day construction for nurse rostering.
///
/// Iterates day by day, shift by shift, and assigns employees greedily.
RosteringSolution construct_greedy(RosteringData const& data,
                                   RosteringCostEvaluator const& evaluator);

}  // namespace coso
