#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <random>

namespace coso {

/// Selective Route Exchange (SREX) crossover from Nagata & Bräysy (2009).
///
/// Produces an offspring that inherits complete routes from parent1 and fills
/// remaining clients using structure from parent2:
///
/// 1. Select a random subset of routes from parent1 (1..max_routes routes).
/// 2. Copy parent2 as the offspring.
/// 3. Remove all clients from the selected parent1 routes from the offspring.
/// 4. Replace empty vehicle slots in the offspring with the selected routes.
/// 5. Reinsert any missing clients via cheapest insertion.
/// 6. (Duplicate clients are impossible by construction.)
///
/// @param parent1   First parent solution.
/// @param parent2   Second parent solution.
/// @param data      Problem data.
/// @param eval      Cost evaluator (used for cheapest insertion).
/// @param rng       Random number generator.
/// @param max_routes Maximum number of routes to select from parent1 (default 3).
/// @return A new offspring solution.
Solution srex_crossover(Solution const& parent1, Solution const& parent2, ProblemData const& data,
                        CostEvaluator const& eval, std::mt19937& rng, int max_routes = 3);

}  // namespace coso
