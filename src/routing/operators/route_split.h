#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <cstdint>

namespace coso {

/// Route split operator for VRP local search.
///
/// Splits a long route into two shorter routes at the optimal split point.
/// For each non-trivial route (size >= 2), tries all possible split positions
/// and evaluates the cost of two routes vs the original single route.
///
/// A split at position p in route [c0, c1, ..., cn-1] produces:
///   - Route A: [c0, ..., cp]      (first p+1 clients)
///   - Route B: [cp+1, ..., cn-1]  (remaining clients)
///
/// The split is beneficial when it reduces capacity violations, improves
/// distance (e.g., the original route doubles back on itself), or when the
/// penalty savings outweigh the additional fixed vehicle cost.
///
/// Requires an empty vehicle slot for the new route. If no empty vehicle
/// of the same type exists, the route cannot be split.
///
/// Follows the standard operator interface: find_best_move() returns true
/// if an improving split exists; apply() executes it.
class RouteSplit {
public:
    /// Scan all routes and find the best improving split.
    ///
    /// @return true if an improving split was found (delta < 0).
    [[nodiscard]] bool find_best_move(Solution const& sol, CostEvaluator const& eval,
                                      ProblemData const& data);

    /// Apply the stored best split to the solution.
    /// Precondition: find_best_move() returned true.
    void apply(Solution& sol) const;

    /// The cost delta of the best split found (negative = improving).
    [[nodiscard]] int64_t best_delta() const noexcept { return best_delta_; }

private:
    int64_t best_delta_ = 0;

    int source_route_ = -1;  ///< Route to split.
    int split_pos_ = -1;     ///< Split after this position (0..size-2).
    int target_route_ = -1;  ///< Empty route to receive the second half.
};

}  // namespace coso
