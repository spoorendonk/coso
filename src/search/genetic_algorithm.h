#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/stop_criterion.h"

#include <cstdint>
#include <random>

namespace coso {

/// Hybrid Genetic Search (HGS) for routing problems.
///
/// Implements the HGS algorithm from Vidal et al.:
///   1. Initialize population with diverse solutions (construction heuristics
///      + local search education).
///   2. Main loop:
///      a. Select two parents via binary tournament (from Population).
///      b. Apply SREX crossover to produce offspring.
///      c. Educate offspring: run local search.
///      d. Add offspring to population (diversity + survivor selection).
///      e. Update penalty weights via PenaltyManager.
///   3. Stop when StopCriterion triggers.
///   4. Return best feasible solution found.
///
/// Uses PenaltyManager for adaptive penalty tuning, Population for
/// diversity-managed sub-populations, srex_crossover for recombination,
/// and LocalSearch for education of offspring.
class GeneticAlgorithm {
public:
    /// Construct an HGS engine for the given problem instance.
    ///
    /// @param data  The compiled problem data.
    explicit GeneticAlgorithm(ProblemData const& data);

    /// Run HGS until the stop criterion triggers.
    ///
    /// @param eval  Cost evaluator (defines initial penalty weights).
    /// @param stop  Stop criterion (modified: iteration/improvement called).
    /// @return The best feasible solution found, or the best infeasible
    ///         solution if no feasible solution was found.
    [[nodiscard]] Solution run(CostEvaluator const& eval, StopCriterion& stop);

    /// Set the maximum size for the feasible sub-population.
    /// Default: 25.
    void set_population_size(int feasible, int infeasible);

    /// Set the number of initial solutions to generate.
    /// Default: 4 * max_feasible.
    void set_initial_population_size(int n);

    /// Set the random seed.
    void set_seed(uint64_t seed);

private:
    ProblemData const* data_;
    std::mt19937 rng_{42};

    // Population sizing.
    int max_feasible_ = 25;
    int max_infeasible_ = 25;
    int initial_pop_size_ = 0;  // 0 means auto = 4 * max_feasible_
};

} // namespace coso
