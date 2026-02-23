#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <cstdint>
#include <vector>

namespace coso {

/// SWAP* operator for VRP local search.
///
/// For each pair of routes (r1, r2), considers removing client u from r1 and
/// client v from r2, then reinserting u at the best position in r2 and v at
/// the best position in r1.  The best insertion positions are precomputed in
/// a cache, enabling O(1) lookup per (client, route) pair.
///
/// This is more powerful than Exchange(1,1) because swapped clients are not
/// constrained to occupy each other's positions -- they go to their
/// individually optimal positions in the target route.
///
/// Also considers the degenerate cases:
///   - Remove u from r1, insert at best position in r2 (relocate, v unused).
///   - Remove v from r2, insert at best position in r1 (relocate, u unused).
///
/// The neighbourhood is restricted to granular neighbours when available.
///
/// Reference: Vidal et al., "A hybrid genetic search for the CVRP" (2012),
/// Section 3.2.
class SwapStar {
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
    /// Best insertion position cache entry for one client in one route.
    struct InsertPos {
        int pos    = -1;     ///< Best insertion position (0..route.size()).
        int64_t delta = 0;   ///< Cost delta of inserting at that position.
    };

    /// Compute the best insertion position for a client in a route.
    static InsertPos best_insert(Route const& route,
                                 CostEvaluator const& eval,
                                 int client);

    int64_t best_delta_ = 0;

    // Stored move description.
    enum MoveType { kSwap, kRelocateAtoB, kRelocateBtoA };

    MoveType move_type_ = kSwap;
    int route_a_  = -1;
    int route_b_  = -1;
    int client_u_ = -1;
    int client_v_ = -1;
    int pos_u_    = -1;
    int pos_v_    = -1;
    int insert_u_ = -1;
    int insert_v_ = -1;
};

} // namespace coso
