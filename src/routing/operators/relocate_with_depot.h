#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <cstdint>
#include <vector>

namespace coso {

// DEPOT_VISIT sentinel is defined in routing/route.h.

/// Relocate-with-depot-insert operator for multi-trip VRP.
///
/// In multi-trip VRP a vehicle can return to the depot mid-route to reload.
/// DEPOT_VISIT markers in the route's client list encode sub-trip boundaries.
/// For example, [c0, c1, DEPOT_VISIT, c2, c3] means the vehicle serves c0
/// and c1, returns to depot (reload), then serves c2 and c3.
///
/// Considers two types of moves within each route:
///
///   1. **Insert depot visit** (split): insert a depot return/departure
///      between two consecutive clients, creating two sub-trips.  Each
///      sub-trip has its own load budget.  The distance cost changes by
///      dist(ci, depot) + dist(depot, ci+1) - dist(ci, ci+1).
///
///   2. **Remove depot visit** (merge): remove an existing DEPOT_VISIT
///      marker, merging two sub-trips into one.  This saves depot-detour
///      distance if the merged trip fits within capacity.
///
/// The operator respects the vehicle type's max_reloads limit: a depot
/// visit is only inserted if the current number of depot visits in the
/// route is below max_reloads.
///
/// Follows the same interface as Exchange operators.
class RelocateWithDepot {
public:
    /// Scan all routes for the best improving depot-insert or depot-remove
    /// move.
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

    // -------------------------------------------------------------------
    //  Static helpers for multi-trip support
    // -------------------------------------------------------------------

    /// Split a client sequence at DEPOT_VISIT markers into sub-trips.
    /// Each sub-trip is a contiguous span of real client indices.
    [[nodiscard]] static std::vector<std::vector<int>>
    split_into_trips(std::vector<int> const& clients);

    /// Count the number of DEPOT_VISIT markers in a client sequence.
    [[nodiscard]] static int count_depot_visits(
        std::vector<int> const& clients);

private:
    int64_t best_delta_ = 0;

    enum MoveType { kInsertDepot, kRemoveDepot };

    MoveType move_type_ = kInsertDepot;
    int route_   = -1;
    int pos_     = -1;  // position to insert depot (between pos-1 and pos)
                        // or position of DEPOT_VISIT to remove
};

} // namespace coso
