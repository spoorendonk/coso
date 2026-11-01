#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <cstdint>

namespace coso {

class StopCriterion;

/// SWAP* operator for VRP local search.
///
/// For each pair of routes (A, B), considers removing client u from A and
/// client v from B, then reinserting u at the best position in B and v at
/// the best position in A.  This is more powerful than Exchange(1,1) because
/// swapped clients are not constrained to occupy each other's positions.
///
/// Follows Vidal (2022), "Hybrid genetic search for the CVRP: Open-source
/// implementation and SWAP* neighborhood", Computers & OR 140, §4: for each
/// route pair it precomputes every client's removal cost and its three
/// cheapest insertion positions in the other route.  The best place for v in
/// A once u has left is then either one of v's top-3 positions not adjacent
/// to u, or u's own place, so each (u, v) is priced in O(1) cached lookups
/// plus the in-place cost.  The load penalty of the exchange is priced
/// exactly from u's place, as in the paper; the insertion and removal
/// costs leave it out.  The best estimated swap of each route pair is then
/// priced exactly, so best_delta() is always the true change in cost.
///
/// Deviations from the paper, for this engine:
///   - Candidate route pairs come from granular neighbours, not from
///     overlapping polar sectors.
///   - The removal and top-3 caches are rebuilt per route pair on every call,
///     not kept across calls with per-route invalidation.
///   - Costs are the full penalised costs (time warp included), not
///     distance alone.
///
/// Also considers the degenerate cases:
///   - Remove u from A, insert at best position in B (relocate, v unused).
///   - Remove v from B, insert at best position in A (relocate, u unused).
class SwapStar {
public:
    /// Scan the neighbourhood and find the best improving move.
    ///
    /// When `stop` is given, it is checked before each route pair.  Once it
    /// fires the scan ends early and keeps the best move found so far, which
    /// is still an exactly priced improving move if one was found.
    ///
    /// @return true if an improving move was found (delta < 0).
    [[nodiscard]] bool find_best_move(Solution const& sol, CostEvaluator const& eval,
                                      ProblemData const& data, StopCriterion const* stop = nullptr);

    /// Apply the stored best move to the solution.
    /// Precondition: find_best_move() returned true.
    void apply(Solution& sol) const;

    /// The cost delta of the best move found (negative = improving).
    [[nodiscard]] int64_t best_delta() const noexcept { return best_delta_; }

private:
    int64_t best_delta_ = 0;

    // Stored move description.
    enum MoveType { Swap, RelocateAtoB, RelocateBtoA };

    MoveType move_type_ = Swap;
    int route_a_ = -1;
    int route_b_ = -1;
    int client_u_ = -1;
    int client_v_ = -1;
    int pos_u_ = -1;
    int pos_v_ = -1;
    int insert_u_ = -1;
    int insert_v_ = -1;
};

}  // namespace coso
