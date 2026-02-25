#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/acceptance.h"
#include "search/stop_criterion.h"

#include <optional>
#include <random>

namespace coso {

/// Iterated Local Search (ILS) for routing problems.
///
/// Algorithm:
///   1. Construct initial solution (Clarke-Wright savings)
///   2. Local search to local optimum
///   3. Perturb: ruin-and-recreate (remove k clients, reinsert greedily)
///   4. Local search again
///   5. Accept or reject via configurable acceptance criterion
///   6. Repeat until stop criterion met
///   7. Return best solution found
///
/// Perturbation — Ruin-and-recreate:
///   - Ruin: select a random seed client, remove it and its k-1 nearest
///     neighbours from the solution. k ~ 10-30% of clients (randomized).
///   - Recreate: greedily reinsert removed clients at their cheapest position.
///
/// Acceptance — configurable (default: Late Acceptance Hill Climbing):
///   - LateAcceptance: accepts if cost <= cost from L iterations ago.
///   - SimulatedAnnealing: accepts worse with probability exp(-delta/T).
///   - RecordToRecord: accepts if cost <= best + threshold.
class IteratedLocalSearch {
public:
    /// Construct an ILS engine for the given problem instance.
    ///
    /// @param data  The compiled problem data.
    /// @param seed  Random seed for perturbation (default: 42).
    explicit IteratedLocalSearch(ProblemData const& data,
                                unsigned int seed = 42);

    /// Run ILS until the stop criterion triggers.
    ///
    /// @param eval  Cost evaluator (defines objective + penalties).
    /// @param stop  Stop criterion (modified: iteration/improvement called).
    /// @param outer_stop  Optional global stop criterion to enforce in
    ///                    addition to the local ILS stop budget.
    /// @return The best solution found during the search.
    [[nodiscard]] Solution run(CostEvaluator const& eval,
                               StopCriterion& stop,
                               StopCriterion* outer_stop = nullptr);

    /// Set the acceptance criterion. If not called, defaults to LAHC(5000).
    void set_acceptance(AcceptanceCriterion criterion);

    /// Minimum fraction of clients to remove in ruin (default 0.1).
    void set_ruin_fraction_min(double frac) { ruin_frac_min_ = frac; }

    /// Maximum fraction of clients to remove in ruin (default 0.3).
    void set_ruin_fraction_max(double frac) { ruin_frac_max_ = frac; }

private:
    ProblemData const* data_;
    std::mt19937 rng_;

    // Parameters.
    std::optional<AcceptanceCriterion> acceptance_;
    double ruin_frac_min_ = 0.1;
    double ruin_frac_max_ = 0.3;

    /// Ruin-and-recreate perturbation.  Modifies sol in place.
    void perturb_(Solution& sol, CostEvaluator const& eval);

    /// Ruin phase: remove k clients near a random seed.
    /// Returns the list of removed client indices.
    std::vector<int> ruin_(Solution& sol);

    /// Recreate phase: greedily reinsert clients at cheapest positions.
    void recreate_(Solution& sol, std::vector<int> const& removed,
                   CostEvaluator const& eval);
};

} // namespace coso
