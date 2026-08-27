#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/route.h"
#include "routing/solution.h"

#include <cstdint>
#include <string_view>

namespace coso::debug {

/// Recompute prefix/suffix arrays from scratch for the given route and
/// compare with stored values.  Asserts they match.
///
/// Intended for debug builds to catch incremental evaluation drift.
void assert_route_consistency(Route const& route, ProblemData const& data);

/// For each route, verify route consistency.  Then recompute total cost
/// from scratch and compare with solution.cost().  Verify all client
/// assignments are correct (each client in exactly one route or unassigned).
void assert_solution_consistency(Solution const& sol, CostEvaluator const& eval,
                                 ProblemData const& data);

/// Verify that a predicted cost delta matches the actual cost change.
/// Asserts |predicted - actual| == 0.
void assert_cost_delta(int64_t predicted, int64_t actual, std::string_view context = "");

}  // namespace coso::debug
