#pragma once

#include "rostering/cost_evaluator.h"
#include "rostering/rostering_data.h"
#include "rostering/rostering_solution.h"

namespace coso {

// Both constructions never make an assignment that breaks unavailability,
// max_consecutive_days, a forbidden sequence, max_weekends or
// max_total_minutes, and finish by topping each employee up to
// min_total_minutes.  An employee they cannot top up is left short, so the
// returned roster is_feasible() == false rather than silently wrong.

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
