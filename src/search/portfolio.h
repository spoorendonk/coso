#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/stop_criterion.h"

#include <cstdint>

namespace coso {

/// Portfolio solver: runs multiple search strategies and combines results.
///
/// Sequential mode (default):
///   1. Run ILS for a fixed iteration budget (fast local search phase).
///   2. Run GA (HGS) for the remaining time/iteration budget.
///   3. Keep the best solution found across both strategies.
///   4. Apply SolutionFinalizer to polish and guarantee feasibility.
///   5. Return the finalized best solution.
///
/// ILS converges quickly to a good local optimum.  GA then uses
/// population-based search with crossover to explore beyond ILS's
/// local optimum for the remaining budget.
///
/// Future: parallel mode with TBB, running ILS and HGS concurrently;
/// seeding GA population with ILS best solution.
class PortfolioSolver {
public:
    /// Construct a portfolio solver for the given problem instance.
    explicit PortfolioSolver(ProblemData const& data);

    /// Run the portfolio solver until the stop criterion triggers.
    ///
    /// @param eval  Cost evaluator (defines objective + penalties).
    /// @param stop  Stop criterion (the overall time/iteration budget).
    /// @return The best solution found across all strategies, after
    ///         finalization.
    [[nodiscard]] Solution run(CostEvaluator const& eval, StopCriterion& stop);

    /// Set the random seed (passed to both ILS and GA).
    void set_seed(uint64_t seed);

private:
    ProblemData const* data_;
    uint64_t seed_ = 42;
};

}  // namespace coso
