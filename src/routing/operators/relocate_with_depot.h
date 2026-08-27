#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <cstdint>

namespace coso {

/// Sentinel value representing a depot visit (reload) within a route sequence.
///
/// Multi-trip routes contain kDepotVisit markers to indicate where the vehicle
/// returns to the depot, reloads, and departs again.  This constant is used
/// when building or interpreting multi-trip sequences externally (e.g., in
/// result reporting).  Within the solver, multi-trip is implemented by
/// splitting one logical vehicle trip into separate Route objects.
///
/// kDepotVisit == -1.  Valid client indices are >= 0, so this is unambiguous.
inline constexpr int kDepotVisit = -1;

/// Check whether a value in a route sequence is a depot visit marker.
[[nodiscard]] inline bool is_depot_visit(int v) noexcept {
    return v == kDepotVisit;
}

// ---------------------------------------------------------------------------
//  RelocateWithDepot — insert/remove depot visits for multi-trip VRP
// ---------------------------------------------------------------------------

/// Operator for multi-trip VRP that splits or merges routes.
///
/// Multi-trip means a vehicle can perform multiple trips within its shift,
/// returning to the depot to reload between trips.  Each trip is represented
/// as a separate Route in the Solution (sharing the same vehicle type).
///
/// Two move types:
///   1. **Split (insert depot visit)**: Take a route and split it at some
///      position, creating two routes.  The first route gets clients before
///      the split point, the second gets clients after.  The second route
///      is placed in an empty vehicle slot of the same type.  This resets
///      the load at the split point, potentially fixing capacity violations,
///      at the cost of extra depot-return/depot-depart distances.
///
///   2. **Merge (remove depot visit)**: Take two routes of the same vehicle
///      type and merge them into one, appending the second route's clients
///      after the first route's clients.  This removes the depot detour but
///      may introduce capacity violations.
///
/// The operator respects `VehicleTypeData::reload_depot` (must be >= 0 for
/// the vehicle to support multi-trip) and `max_reloads` (limits the number
/// of trips per vehicle beyond the first).
///
/// Designed for steepest-descent local search: find_best_move() scans all
/// possibilities, stores the single best improving move, and apply() executes
/// it.
class RelocateWithDepot {
public:
    /// Scan all routes for the best split or merge move.
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

    enum class MoveType { SPLIT, MERGE };

    MoveType move_type_ = MoveType::SPLIT;

    // For SPLIT: split route_a_ at position split_pos_.
    //   Clients [0..split_pos_) stay in route_a_.
    //   Clients [split_pos_..n) go to empty_route_ (an empty route slot).
    int route_a_ = -1;
    int split_pos_ = -1;
    int empty_route_ = -1;  // destination for the second half

    // For MERGE: append route_b_'s clients after route_a_'s clients.
    int route_b_ = -1;

    /// Find an empty route slot for the given vehicle type.
    /// Returns -1 if none available.
    [[nodiscard]] static int find_empty_route(Solution const& sol, ProblemData const& data,
                                              int vehicle_type, int exclude_route = -1);

    /// Count how many non-empty routes exist for a given vehicle type.
    [[nodiscard]] static int count_trips(Solution const& sol, ProblemData const& data,
                                         int vehicle_type);
};

}  // namespace coso
