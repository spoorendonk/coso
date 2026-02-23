#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

namespace coso {

/// Construction heuristics that build an initial feasible routing solution.
///
/// Both heuristics take a ProblemData and CostEvaluator, and return a Solution
/// with all clients assigned (if sufficient vehicle capacity exists).
/// They respect vehicle capacity constraints (LoadResource) and handle
/// multiple vehicle types.
namespace construction {

/// Nearest-neighbour construction heuristic.
///
/// Starts each route from the depot.  Repeatedly inserts the nearest
/// unvisited client that does not violate the vehicle's capacity.
/// When no more clients fit in the current route, starts a new route
/// on the next available vehicle.
///
/// Simple, fast, produces a decent starting solution.
///
/// @param data  The problem instance.
/// @param eval  Cost evaluator (used for distance profile selection).
/// @return A Solution with clients assigned to routes.
[[nodiscard]] Solution nearest_neighbour(ProblemData const& data,
                                         CostEvaluator const& eval);

/// Clarke-Wright savings construction heuristic.
///
/// 1. Start with each client in its own singleton route (depot -> client -> depot).
/// 2. Compute savings s(i,j) = d(depot,i) + d(depot,j) - d(i,j) for all
///    client pairs.
/// 3. Sort savings in decreasing order.
/// 4. For each saving, merge the two routes by connecting clients i and j
///    if: (a) i is the last client in one route and j is the first in another
///    (or vice versa), (b) merging does not violate capacity, and (c) both
///    routes belong to the same vehicle type (or can be consolidated).
///
/// Produces better solutions than nearest-neighbour in most cases.
///
/// @param data  The problem instance.
/// @param eval  Cost evaluator (used for distance profile selection).
/// @return A Solution with clients assigned to routes.
[[nodiscard]] Solution clarke_wright(ProblemData const& data,
                                     CostEvaluator const& eval);

} // namespace construction
} // namespace coso
