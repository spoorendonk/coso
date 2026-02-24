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
///   1. Run ILS for a fraction of the time budget.
///   2. Seed GA's population with the ILS best solution.
///   3. Run GA (HGS) for the remaining time budget.
///   4. Apply SolutionFinalizer to the overall best solution.
///   5. Return the best feasible solution found.
///
/// The time budget is split between ILS and GA.  By default, ILS gets 40%
/// and GA gets 60%, since GA benefits more from search time (population
/// diversity + crossover need iterations to converge).
///
/// Future: parallel mode with TBB, running ILS and HGS concurrently.
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

    /// Set the fraction of time allocated to ILS (default: 0.4).
    /// The remaining fraction goes to GA.
    /// Must be in (0, 1).
    void set_ils_fraction(double frac);

private:
    ProblemData const* data_;
    uint64_t seed_ = 42;
    double ils_fraction_ = 0.4;
};

} // namespace coso
