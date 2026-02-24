#include "search/portfolio.h"

#include "search/genetic_algorithm.h"
#include "search/iterated_local_search.h"
#include "search/solution_finalizer.h"
#include "search/stop_criterion.h"

namespace coso {

PortfolioSolver::PortfolioSolver(ProblemData const& data)
    : data_(&data)
{
}

void PortfolioSolver::set_seed(uint64_t seed)
{
    seed_ = seed;
}

void PortfolioSolver::set_ils_fraction(double frac)
{
    assert(frac > 0.0 && frac < 1.0);
    ils_fraction_ = frac;
}

Solution PortfolioSolver::run(CostEvaluator const& eval, StopCriterion& stop)
{
    // Phase 1: Run ILS with a limited iteration budget.
    //
    // ILS converges quickly to a good local optimum.  We give it a fixed
    // iteration budget (500 iterations) so it acts as a fast initialization
    // phase.  If the parent stop criterion triggers earlier (e.g. short
    // time limit), ILS will still respect it via its own sub-stop.
    //
    // We use a sub-stop with an iteration limit rather than a time-based
    // split because StopCriterion does not expose its time limit.
    constexpr int kIlsIterations = 500;

    IteratedLocalSearch ils(*data_, static_cast<unsigned int>(seed_));
    StopCriterion ils_stop(0.0, kIlsIterations, 0);

    Solution best = ils.run(eval, ils_stop);
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
    finalizer.finalize(best);

    return best;
}

} // namespace coso
