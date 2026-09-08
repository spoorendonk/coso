#pragma once

#include "rostering/rostering_data.h"
#include "rostering/rostering_solution.h"
#include "rostering/cost_evaluator.h"

namespace coso {

/// First-Fit-Decreasing construction for nurse rostering.
///
/// Sorts demands by required employees (descending) and greedily assigns the
/// cheapest available employee to each unmet demand slot.
RosteringSolution construct_ffd(RosteringData const& data,
                                 RosteringCostEvaluator const& evaluator);

/// Greedy day-by-day construction for nurse rostering.
///
/// Iterates day by day, shift by shift, and assigns employees greedily
/// while respecting hard constraints (unavailability, consecutive days,
/// rest between shifts).
RosteringSolution construct_greedy(RosteringData const& data,
                                    RosteringCostEvaluator const& evaluator);

}  // namespace coso
