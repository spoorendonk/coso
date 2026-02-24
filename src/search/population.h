#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

namespace coso {

/// Population with diversity management for HGS (Hybrid Genetic Search).
///
/// Maintains two sub-populations: feasible and infeasible solutions.
/// Uses **broken-pairs diversity** to measure how different two solutions are,
/// and **biased fitness** that combines cost rank with diversity contribution
/// to keep a diverse set of high-quality solutions.
///
/// Broken-pairs diversity between two solutions counts the number of
/// consecutive client pairs (edges) that appear in one solution but not
/// the other.
///
/// Biased fitness for individual i:
///   bf(i) = cost_rank(i) + (1 - num_close / pop_size) * diversity_rank(i)
/// where num_close is the number of closest neighbours used for diversity
/// contribution, and ranks are 0-based (lower = better).
///
/// When a sub-population exceeds its maximum size, the solution with the
/// worst (highest) biased fitness is removed.
///
/// Parent selection uses binary tournament on biased fitness.
class Population {
public:
    /// @param data           The problem data (needed for num_clients).
    /// @param max_feasible   Maximum size of the feasible sub-population.
    /// @param max_infeasible Maximum size of the infeasible sub-population.
    /// @param num_close      Number of closest neighbours for diversity
    ///                       contribution (default: floor(pop_size * 0.2),
    ///                       clamped to at least 1).  Pass 0 for automatic.
    explicit Population(ProblemData const& data,
                        int max_feasible = 25,
                        int max_infeasible = 25,
                        int num_close = 0);

    /// Add a solution to the appropriate sub-population (feasible or
    /// infeasible based on sol.feasible()).  If the sub-population exceeds
    /// its max size, the worst-fitness solution is removed.
    void add(Solution sol, CostEvaluator const& eval);

    /// Select a parent using binary tournament on biased fitness.
    /// Draws from the combined population (feasible + infeasible).
    /// Requires at least one solution in the population.
    [[nodiscard]] Solution const& select_parent(std::mt19937& rng) const;

    /// The best feasible solution (lowest cost).
    /// Requires has_feasible().
    [[nodiscard]] Solution const& best_feasible() const;

    /// Whether there is at least one feasible solution.
    [[nodiscard]] bool has_feasible() const noexcept {
        return !feasible_.empty();
    }

    /// Number of feasible solutions.
    [[nodiscard]] int feasible_size() const noexcept {
        return static_cast<int>(feasible_.size());
    }

    /// Number of infeasible solutions.
    [[nodiscard]] int infeasible_size() const noexcept {
        return static_cast<int>(infeasible_.size());
    }

    /// Total population size.
    [[nodiscard]] int size() const noexcept {
        return feasible_size() + infeasible_size();
    }

private:
    struct Individual {
        Solution sol;
        int64_t cost;                    ///< Penalized cost.
        std::vector<std::pair<int, int>> edges;  ///< Sorted edge set for diversity.
    };

    using SubPop = std::vector<Individual>;

    ProblemData const* data_;
    int max_feasible_;
    int max_infeasible_;
    int num_close_;

    SubPop feasible_;
    SubPop infeasible_;

    /// Extract the sorted edge set from a solution.
    /// An edge is an ordered pair (a, b) with a < b, for each consecutive
    /// pair of clients in each route.  Depot edges are excluded.
    [[nodiscard]] static std::vector<std::pair<int, int>>
    extract_edges(Solution const& sol);

    /// Broken-pairs distance between two individuals.
    [[nodiscard]] static int broken_pairs(Individual const& a,
                                          Individual const& b);

    /// Compute biased fitness for each member of a sub-population.
    /// Returns a vector of fitness values (lower = better).
    [[nodiscard]] std::vector<double>
    biased_fitness(SubPop const& pop) const;

    /// Remove the worst individual from a sub-population.
    void survivor_selection(SubPop& pop, int max_size);

    /// Binary tournament: pick two random indices from combined pop,
    /// return reference to the one with better biased fitness.
    [[nodiscard]] Solution const&
    tournament(std::mt19937& rng) const;
};

} // namespace coso
