#pragma once

#include "rostering/cost_evaluator.h"
#include "rostering/rostering_data.h"
#include "rostering/rostering_solution.h"

#include <functional>

namespace coso {

// Both constructions never make an assignment that breaks unavailability,
// max_consecutive_days, a forbidden sequence, max_weekends or
// max_total_minutes, and finish with top_up_min_minutes.  An employee they
// cannot top up is left short, so the returned roster is_feasible() == false
// rather than silently wrong.

/// Whether a top-up may assign employee e on day d (replanning's unlocked
/// cells).  An empty filter allows every cell.
using CellFilter = std::function<bool(int e, int d)>;

/// Bring every employee up to min_total_minutes, which the demand-driven
/// passes never aim at, adding shifts only on cells `may_assign` allows and
/// never removing one.  It tries greedy first, each step adding the
/// feasible (day, shift) with the cheapest cover effect, never a hard
/// over-cover.  When greedy falls short it is undone and a depth-first
/// search over the employee's open days looks for a set that reaches the
/// minimum.  The search is exact but budgeted per employee; an employee it
/// cannot top up within the budget keeps greedy's partial top-up and is left
/// short, so a miss on a large instance is not proof none is reachable.
void top_up_min_minutes(RosteringData const& data, RosteringSolution& sol,
                        CellFilter const& may_assign = {});

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
