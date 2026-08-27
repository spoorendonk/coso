#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

namespace coso {

class StopCriterion;

/// Local search engine for routing problems.
///
/// Iteratively applies improving moves until a local optimum is reached.
/// Uses first-improvement descent by default: operators are tried in a fixed
/// order (Exchange10, Exchange11, Exchange20, SwapTails); as soon as any
/// operator finds an improving move, it is applied and the search restarts
/// from the first operator.
///
/// The neighbourhood is restricted to granular neighbours (k-nearest clients)
/// when available in ProblemData, falling back to all pairs otherwise.
///
/// Convergence is guaranteed: each applied move strictly decreases the
/// penalized cost, and the set of possible solutions is finite.
class LocalSearch {
public:
    /// Construct a local search engine for the given problem instance.
    explicit LocalSearch(ProblemData const& data);

    /// Run local search on the solution until a local optimum is reached.
    ///
    /// Modifies sol in place.  On return, no single Exchange10, Exchange11,
    /// Exchange20, or SwapTails move can improve the penalized cost.
    void run(Solution& sol, CostEvaluator const& eval, StopCriterion* stop = nullptr);

    /// Number of improving moves applied in the last run() call.
    [[nodiscard]] int last_num_moves() const noexcept { return last_num_moves_; }

    /// Number of iterations (operator scans) in the last run() call.
    [[nodiscard]] int last_num_iters() const noexcept { return last_num_iters_; }

private:
    ProblemData const* data_;
    int last_num_moves_ = 0;
    int last_num_iters_ = 0;
};

}  // namespace coso
