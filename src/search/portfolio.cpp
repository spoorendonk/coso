#include "search/portfolio.h"

#include "search/genetic_algorithm.h"
#include "search/iterated_local_search.h"
#include "search/solution_finalizer.h"
#include "search/stop_criterion.h"

namespace coso {

PortfolioSolver::PortfolioSolver(ProblemData const& data) : data_(&data) {}

void PortfolioSolver::set_seed(uint64_t seed) {
    seed_ = seed;
}

Solution PortfolioSolver::run(CostEvaluator const& eval, StopCriterion& stop) {
    // Phase 1: Run ILS as a short warm-start phase.
    //
    // ILS converges quickly to a good local optimum.  We bound it by both
    // iterations and a small wall-clock budget so GA/finalization still get
    // time, especially for short solve limits used in benchmarks.
    //
    // Keep ILS strictly short; GA handles the remaining budget.
    constexpr int kIlsIterations = 100;
    constexpr double kIlsTimeLimitS = 1.0;

    IteratedLocalSearch ils(*data_, static_cast<unsigned int>(seed_));
    StopCriterion ils_stop(kIlsTimeLimitS, kIlsIterations, 0);

    Solution best = ils.run(eval, ils_stop, &stop);
    int64_t best_cost = best.cost(eval);

    // Phase 2: Run GA (HGS) for the remaining budget.
    //
    // GA uses population-based search with crossover and local search
    // education.  It benefits from more time and can escape local optima
    // that ILS gets stuck in.  We pass the parent stop criterion directly
    // so GA uses whatever budget remains.
    if (!stop.should_stop()) {
        GeneticAlgorithm ga(*data_);
        ga.set_population_size(25, 25);
        ga.set_seed(seed_);

        Solution ga_sol = ga.run(eval, stop);
        int64_t ga_cost = ga_sol.cost(eval);

        if (ga_cost < best_cost) {
            best = std::move(ga_sol);
            best_cost = ga_cost;
        }
    }

    // Phase 3: Finalize the best solution.
    //
    // SolutionFinalizer runs local search with very high penalties to
    // polish the solution and repair any remaining infeasibilities.
    SolutionFinalizer finalizer(*data_);
    if (!stop.should_stop()) {
        finalizer.finalize(best, &stop);
    } else if (!stop.has_work_limit()) {
        // If the global budget is exhausted, still give finalization a short
        // rescue window so it can reduce obvious infeasibilities.
        StopCriterion rescue_stop(0.5);
        finalizer.finalize(best, &rescue_stop);
    }

    return best;
}

}  // namespace coso
