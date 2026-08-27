#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <cstdint>

namespace coso {

/// Insert an unserved optional client at its best position in any route.
///
/// Scans all unassigned clients that are optional (required == false).
/// For each, evaluates inserting it at every position in every route.
/// Accepts if the cost delta is negative (i.e., the prize gain exceeds
/// the distance and penalty costs of insertion).
///
/// Follows the same operator interface as Exchange operators:
/// find_best_move() returns true if an improving insertion exists;
/// apply() executes the best insertion found.
class InsertOptional {
public:
    /// Scan all unserved optional clients and find the best insertion.
    ///
    /// @return true if an improving insertion was found (delta < 0).
    [[nodiscard]] bool find_best_move(Solution const& sol, CostEvaluator const& eval,
                                      ProblemData const& data);

    /// Apply the stored best move to the solution.
    /// Precondition: find_best_move() returned true.
    void apply(Solution& sol) const;

    /// The cost delta of the best move found (negative = improving).
    [[nodiscard]] int64_t best_delta() const noexcept { return best_delta_; }

private:
    int64_t best_delta_ = 0;

    int client_ = -1;  ///< Client to insert.
    int route_ = -1;   ///< Target route index.
    int pos_ = -1;     ///< Insertion position in the target route.
};

// ---------------------------------------------------------------------------

/// Remove a served optional client from its route.
///
/// Scans all routes for optional clients (required == false).
/// For each, evaluates the cost delta of removing it.  Accepts if the
/// distance savings exceed the lost prize (delta < 0).
///
/// This is useful when a previous perturbation or penalty change makes
/// a marginal optional client no longer worth serving.
class RemoveOptional {
public:
    /// Scan all served optional clients and find the best removal.
    ///
    /// @return true if an improving removal was found (delta < 0).
    [[nodiscard]] bool find_best_move(Solution const& sol, CostEvaluator const& eval,
                                      ProblemData const& data);

    /// Apply the stored best move to the solution.
    /// Precondition: find_best_move() returned true.
    void apply(Solution& sol) const;

    /// The cost delta of the best move found (negative = improving).
    [[nodiscard]] int64_t best_delta() const noexcept { return best_delta_; }

private:
    int64_t best_delta_ = 0;

    int route_ = -1;  ///< Route containing the client to remove.
    int pos_ = -1;    ///< Position of the client in the route.
};

}  // namespace coso
