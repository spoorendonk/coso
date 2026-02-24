#include "search/genetic_algorithm.h"

#include "routing/construction.h"
#include "routing/local_search.h"
#include "search/crossover.h"
#include "search/penalty_manager.h"
#include "search/population.h"

#include <algorithm>

namespace coso {

GeneticAlgorithm::GeneticAlgorithm(ProblemData const& data)
    : data_(&data) {}

Solution GeneticAlgorithm::run(CostEvaluator const& eval, StopCriterion& stop)
{
    // Set up penalty manager with initial weights from the evaluator.
    PenaltyManager penalties;

    // Build the cost evaluator from penalty manager (will evolve over time).
    auto pen_eval = penalties.cost_evaluator();

    // Set up population.
    Population pop(*data_, max_feasible_, max_infeasible_);

    // Set up local search engine.
    LocalSearch ls(*data_);

    // Determine initial population size.
    int init_size = initial_pop_size_ > 0
                        ? initial_pop_size_
                        : 4 * max_feasible_;

    // Seed the population with construction heuristics + education.
    // Alternate between nearest-neighbour and Clarke-Wright for diversity,
    // with random perturbation of the cost evaluator to get varied solutions.
    for (int i = 0; i < init_size && !stop.should_stop(); ++i) {
        Solution sol = (i % 2 == 0)
            ? construction::nearest_neighbour(*data_, pen_eval)
            : construction::clarke_wright(*data_, pen_eval);

        // Educate: run local search to a local optimum.
        ls.run(sol, pen_eval);

        // Register feasibility and update penalties.
        penalties.register_solution(sol.feasible());
        pen_eval = penalties.cost_evaluator();

        pop.add(std::move(sol), pen_eval);
    }

    // Track the best feasible cost found so far.
    int64_t best_cost = pop.has_feasible()
        ? pop.best_feasible().cost(eval)
        : std::numeric_limits<int64_t>::max();

    // Main HGS loop.
    while (!stop.should_stop()) {
        // Need at least 2 solutions for crossover.
        if (pop.size() < 2) {
            // This should not happen after initialization, but handle it.
            auto sol = construction::nearest_neighbour(*data_, pen_eval);
            ls.run(sol, pen_eval);
            penalties.register_solution(sol.feasible());
            pen_eval = penalties.cost_evaluator();
            pop.add(std::move(sol), pen_eval);
            stop.iteration();
            continue;
        }

        // a. Select two parents via binary tournament.
        auto const& parent1 = pop.select_parent(rng_);
        auto const& parent2 = pop.select_parent(rng_);

        // b. Apply SREX crossover to produce offspring.
        auto offspring = srex_crossover(parent1, parent2, *data_, pen_eval,
                                        rng_);

        // c. Educate: run local search on offspring.
        ls.run(offspring, pen_eval);

        // d. Register feasibility and update penalty weights.
        penalties.register_solution(offspring.feasible());
        pen_eval = penalties.cost_evaluator();

        // Check if this offspring improves the best feasible solution.
        if (offspring.feasible()) {
            auto offspring_cost = offspring.cost(eval);
            if (offspring_cost < best_cost) {
                best_cost = offspring_cost;
                stop.improvement();
            }
        }

        // e. Add offspring to population (diversity + survivor selection).
        pop.add(std::move(offspring), pen_eval);

        stop.iteration();
    }

    // Return the best feasible solution, or the best we have.
    if (pop.has_feasible()) {
        return pop.best_feasible();
    }

    // No feasible solution found: return the best from the population.
    // select_parent returns a reference; use it as a fallback.
    return pop.select_parent(rng_);
}

void GeneticAlgorithm::set_population_size(int feasible, int infeasible)
{
    max_feasible_ = feasible;
    max_infeasible_ = infeasible;
}

void GeneticAlgorithm::set_initial_population_size(int n)
{
    initial_pop_size_ = n;
}

void GeneticAlgorithm::set_seed(uint64_t seed)
{
    rng_.seed(seed);
}

} // namespace coso
