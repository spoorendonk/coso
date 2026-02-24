#pragma once

#include "assignment/assignment_data.h"
#include "assignment/assignment_solution.h"
#include "assignment/cost_evaluator.h"

namespace coso {

/// First-Fit-Decreasing construction for nurse rostering.
///
/// Sorts demands by required employees (descending) and greedily assigns the
/// cheapest available employee to each unmet demand slot.
AssignmentSolution construct_ffd(AssignmentData const& data,
                                 AssignmentCostEvaluator const& evaluator);

/// Greedy day-by-day construction for nurse rostering.
///
/// Iterates day by day, shift by shift, and assigns employees greedily
/// while respecting hard constraints (unavailability, consecutive days,
/// rest between shifts).
AssignmentSolution construct_greedy(AssignmentData const& data,
                                    AssignmentCostEvaluator const& evaluator);

} // namespace coso
