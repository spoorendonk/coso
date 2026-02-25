#pragma once

#include "routing/local_search.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

namespace coso {

class StopCriterion;

/// Post-optimization step that runs after the main search.
///
/// The finalizer:
///   1. Runs local search with very high penalty weights so that only feasible
///      moves are accepted (effectively infinite penalty makes any constraint
///      violation prohibitively expensive).
///   2. If the solution is infeasible (e.g. capacity violations remaining from
///      the penalized search), attempts to repair it by removing excess clients
///      from overloaded routes.
///   3. Runs local search again to exploit inter-route moves that may now be
///      profitable after repair.
///
/// The result is a feasible, locally optimal solution with reduced distance.
class SolutionFinalizer {
public:
    explicit SolutionFinalizer(ProblemData const& data);

    /// Finalize the solution in place.
    ///
    /// On return the solution is feasible (load_feasible on every route)
    /// and locally optimal with respect to the high-penalty cost evaluator.
    void finalize(Solution& sol, StopCriterion* stop = nullptr);

private:
    ProblemData const* data_;
    LocalSearch ls_;

    /// Remove clients from overloaded routes until all routes are feasible.
    /// Removed clients become unassigned.
    void repair_infeasible_(Solution& sol);
};

} // namespace coso
