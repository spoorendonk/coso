#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <cstdint>
#include <vector>

namespace coso {

/// Pickup-delivery pair operators for VRP local search.
///
/// These operators move pickup-delivery pairs together, maintaining the
/// precedence constraint (pickup before delivery in the same route).
///
/// Unlike Exchange(N,M) operators which move consecutive segments, pair
/// operators identify pickup-delivery pairs that may be non-consecutive
/// in a route and move both nodes together, finding the best feasible
/// insertion positions that respect precedence.

// ---------------------------------------------------------------------------
//  RelocatePair — Move a pickup-delivery pair to another route (or within)
// ---------------------------------------------------------------------------

/// Relocate a pickup-delivery pair from one route to another.
///
/// For each pickup-delivery request assigned to a route, evaluates removing
/// both the pickup and delivery nodes and reinserting them (pickup before
/// delivery) at the best positions in every candidate target route.
///
/// The operator tries all valid insertion positions: for each position p
/// where the pickup could be inserted, the delivery is tried at every
/// position >= p (to maintain precedence).
///
/// Also considers intra-route moves: removing the pair and reinserting
/// at different positions within the same route.
class RelocatePair {
public:
    /// Scan the neighbourhood and find the best improving move.
    ///
    /// @return true if an improving move was found (delta < 0).
    [[nodiscard]] bool find_best_move(Solution const& sol, CostEvaluator const& eval,
                                      ProblemData const& data);

    /// Apply the stored best move to the solution.
    /// Precondition: find_best_move() returned true.
    void apply(Solution& sol) const;

    /// The cost delta of the best move found (negative = improving).
    [[nodiscard]] int64_t best_delta() const noexcept { return best_delta_; }

private:
    int64_t best_delta_ = 0;

    int request_ = -1;     ///< Request index being moved.
    int from_route_ = -1;  ///< Source route.
    int to_route_ = -1;    ///< Target route.
    int pickup_ = -1;      ///< Pickup client index.
    int delivery_ = -1;    ///< Delivery client index.
    int insert_p_ = -1;    ///< Insertion position for pickup in target.
    int insert_d_ = -1;    ///< Insertion position for delivery in target.
};

// ---------------------------------------------------------------------------
//  SwapPair — Swap two pickup-delivery pairs between routes
// ---------------------------------------------------------------------------

/// Swap two pickup-delivery pairs between different routes.
///
/// For each pair of requests (one in route A, one in route B), evaluates
/// removing both pairs and reinserting them in the opposite routes at
/// the best positions respecting pickup-before-delivery precedence.
class SwapPair {
public:
    /// Scan the neighbourhood and find the best improving move.
    ///
    /// @return true if an improving move was found (delta < 0).
    [[nodiscard]] bool find_best_move(Solution const& sol, CostEvaluator const& eval,
                                      ProblemData const& data);

    /// Apply the stored best move to the solution.
    /// Precondition: find_best_move() returned true.
    void apply(Solution& sol) const;

    /// The cost delta of the best move found (negative = improving).
    [[nodiscard]] int64_t best_delta() const noexcept { return best_delta_; }

private:
    int64_t best_delta_ = 0;

    int request_a_ = -1;  ///< First request (from route A).
    int request_b_ = -1;  ///< Second request (from route B).
    int route_a_ = -1;    ///< Route A index.
    int route_b_ = -1;    ///< Route B index.
    int pickup_a_ = -1;
    int delivery_a_ = -1;
    int pickup_b_ = -1;
    int delivery_b_ = -1;
    // Best insertion positions for pair A in route B.
    int insert_pa_ = -1;
    int insert_da_ = -1;
    // Best insertion positions for pair B in route A.
    int insert_pb_ = -1;
    int insert_db_ = -1;
};

}  // namespace coso
