#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <cstdint>

namespace coso {

/// Exchange operators for VRP local search.
///
/// Each operator scans a neighbourhood of route pairs, evaluates moves using
/// O(1) delta evaluation (via Route's prefix/suffix arrays and CostEvaluator),
/// stores the best improving move found, and can apply it to the solution.
///
/// Designed for steepest-descent local search: find_best_move() returns true
/// if an improving move exists; apply() executes it.
///
/// The neighbourhood is restricted to granular neighbours from ProblemData
/// (k-nearest clients) when available, falling back to all pairs otherwise.

// ---------------------------------------------------------------------------
//  Exchange(1,0) — Relocate: move one client from route A to route B
// ---------------------------------------------------------------------------

/// Relocate one client from one route to another (or within the same route).
///
/// For each client U in each route, evaluates removing U and inserting it at
/// every position in every neighbouring route.  Uses O(1) delta evaluation:
///   delta = eval_remove_cost(routeA, posU) + eval_insert_cost(routeB, posV, U)
///
/// Also considers intra-route moves (same route, different position).
class Exchange10 {
public:
    /// Scan the neighbourhood and find the best improving move.
    ///
    /// @return true if an improving move was found (delta < 0).
    [[nodiscard]] bool find_best_move(Solution const& sol,
                                      CostEvaluator const& eval,
                                      ProblemData const& data);

    /// Apply the stored best move to the solution.
    /// Precondition: find_best_move() returned true.
    void apply(Solution& sol) const;

    /// The cost delta of the best move found (negative = improving).
    [[nodiscard]] int64_t best_delta() const noexcept { return best_delta_; }

private:
    int64_t best_delta_ = 0;

    // Stored move: remove client at from_pos_ in from_route_,
    //              insert at to_pos_ in to_route_.
    int from_route_ = -1;
    int from_pos_   = -1;
    int to_route_   = -1;
    int to_pos_     = -1;
    int client_     = -1;
};

// ---------------------------------------------------------------------------
//  Exchange(1,1) — Swap: swap one client from route A with one from route B
// ---------------------------------------------------------------------------

/// Swap one client from route A with one client from route B.
///
/// For each pair of clients (U in routeA, V in routeB), evaluates replacing
/// U with V in routeA and V with U in routeB.  The delta is computed as:
///   delta = cost(routeA with V instead of U) + cost(routeB with U instead of V)
///         - cost(routeA) - cost(routeB)
///
/// Considers both inter-route and intra-route swaps.
class Exchange11 {
public:
    [[nodiscard]] bool find_best_move(Solution const& sol,
                                      CostEvaluator const& eval,
                                      ProblemData const& data);

    void apply(Solution& sol) const;

    [[nodiscard]] int64_t best_delta() const noexcept { return best_delta_; }

private:
    int64_t best_delta_ = 0;

    int route_a_ = -1;
    int pos_a_   = -1;  // position in route A
    int route_b_ = -1;
    int pos_b_   = -1;  // position in route B
};

// ---------------------------------------------------------------------------
//  Exchange(2,0) — Relocate-pair: move two consecutive clients
// ---------------------------------------------------------------------------

/// Relocate two consecutive clients from route A to route B.
///
/// For each consecutive pair (U, U+1) in a route, evaluates removing both
/// and inserting them (in order) at every position in every neighbouring route.
/// Delta evaluation: remove cost for both clients + insert cost at target.
class Exchange20 {
public:
    [[nodiscard]] bool find_best_move(Solution const& sol,
                                      CostEvaluator const& eval,
                                      ProblemData const& data);

    void apply(Solution& sol) const;

    [[nodiscard]] int64_t best_delta() const noexcept { return best_delta_; }

private:
    int64_t best_delta_ = 0;

    int from_route_ = -1;
    int from_pos_   = -1;  // position of first client in the pair
    int to_route_   = -1;
    int to_pos_     = -1;  // insertion position in target route
};

// ---------------------------------------------------------------------------
//  SwapTails — swap the tail segments of two routes
// ---------------------------------------------------------------------------

/// Swap the tail segments of two routes at given positions.
///
/// Given routeA = [a0, ..., ai, ai+1, ..., an] and
///       routeB = [b0, ..., bj, bj+1, ..., bm],
/// produces routeA' = [a0, ..., ai, bj+1, ..., bm] and
///          routeB' = [b0, ..., bj, ai+1, ..., an].
///
/// This is equivalent to inter-route 2-opt.  Delta evaluation uses full
/// route cost recomputation of the two modified routes (not strictly O(1)
/// in general, but O(1) for the distance component using the triangle of
/// removed/added edges).
class SwapTails {
public:
    [[nodiscard]] bool find_best_move(Solution const& sol,
                                      CostEvaluator const& eval,
                                      ProblemData const& data);

    void apply(Solution& sol) const;

    [[nodiscard]] int64_t best_delta() const noexcept { return best_delta_; }

private:
    int64_t best_delta_ = 0;

    int route_a_  = -1;
    int pos_a_    = -1;  // cut after this position in route A (-1 = before first)
    int route_b_  = -1;
    int pos_b_    = -1;  // cut after this position in route B (-1 = before first)
};

} // namespace coso
