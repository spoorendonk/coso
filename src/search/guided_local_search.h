#pragma once

#include "routing/problem_data.h"
#include "routing/solution.h"

#include <cstdint>
#include <vector>

namespace coso {

/// Guided Local Search (GLS) diversification for routing problems.
///
/// GLS augments the objective with penalties on frequently used edges to
/// escape local optima.  After local search converges, the edge with the
/// highest utility is penalized, and local search is re-run with the
/// augmented cost.
///
/// Utility of edge (i,j) = d(i,j) / (1 + p(i,j)), where d is the
/// distance and p is the penalty counter.  This favours penalizing
/// expensive, under-penalized edges.
///
/// The augmented cost for a solution is:
///   original_cost + lambda * sum_{(i,j) in solution} p(i,j)
///
/// Reference: Voudouris & Tsang, "Guided Local Search and its Application
/// to the Travelling Salesman Problem", European Journal of Operational
/// Research, 1999.
class GuidedLocalSearch {
public:
    /// Construct a GLS instance for the given problem.
    ///
    /// @param data    The compiled problem data.
    /// @param lambda  Penalty weight (scales penalty contribution to cost).
    ///                Typical values: 0.1 to 0.5.  Higher values increase
    ///                diversification pressure.
    explicit GuidedLocalSearch(ProblemData const& data, double lambda = 0.1);

    /// Penalize the most costly under-penalized edge in the current solution.
    ///
    /// Identifies the edge (i,j) in the solution with the highest utility
    /// utility(i,j) = d(i,j) / (1 + p(i,j)) and increments its penalty
    /// counter p(i,j).
    void penalize(Solution const& sol);

    /// Compute the augmented cost of a solution.
    ///
    /// Returns the total GLS penalty contribution:
    ///   lambda * sum_{(i,j) in sol} d(i,j) * p(i,j)
    /// where d(i,j) is the edge distance and p(i,j) is the penalty counter.
    /// This should be added to the base cost from CostEvaluator.
    [[nodiscard]] int64_t augmented_cost(Solution const& sol) const;

    /// Reset all penalty counters to zero.
    void reset();

    /// Get the penalty counter for edge (i,j) in full node numbering.
    [[nodiscard]] int penalty(int from, int to) const;

    /// Get the lambda parameter.
    [[nodiscard]] double lambda() const noexcept { return lambda_; }

    /// Set the lambda parameter.
    void set_lambda(double lambda) noexcept { lambda_ = lambda; }

private:
    ProblemData const* data_;
    double lambda_;
    int num_nodes_;

    /// Flat penalty matrix: penalties_[from * num_nodes_ + to].
    std::vector<int> penalties_;

    /// Collect all edges (as node pairs) used by the solution.
    /// Each edge is (from_node, to_node) in full node numbering,
    /// including depot-to-first-client and last-client-to-depot edges.
    [[nodiscard]] std::vector<std::pair<int, int>> edges_(Solution const& sol) const;
};

}  // namespace coso
