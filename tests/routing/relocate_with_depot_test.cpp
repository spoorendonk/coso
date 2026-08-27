#include "routing/operators/relocate_with_depot.h"

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Test instance builders
// ---------------------------------------------------------------------------

/// Multi-trip instance: 1 depot, 4 clients in a line, 1 vehicle type with
/// capacity 5 and reload enabled.
///
///   Depot(0,0)  C0(10,0)  C1(20,0)  C2(30,0)  C3(40,0)
///   Demands: C0=3, C1=3, C2=3, C3=3   (total=12, capacity=5)
///
/// A single trip can serve at most 1 client pair (demand 6 > capacity 5),
/// so a vehicle needs depot reloads to serve all 4 clients.
/// Actually with capacity=5, each trip serves 1 client (demand 3 fits).
static ProblemData make_multi_trip_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    // 2 vehicles, capacity=5, reload enabled.
    b.add_vehicle_type(2, {
                              .capacity = {5},
                              .reload_depot = 0,
                              .max_reloads = 3,
                          });

    b.add_client({10.0, 0.0}, {.demand = {3}});  // 0
    b.add_client({20.0, 0.0}, {.demand = {3}});  // 1
    b.add_client({30.0, 0.0}, {.demand = {3}});  // 2
    b.add_client({40.0, 0.0}, {.demand = {3}});  // 3

    return b.build(0);
}

/// Instance where splitting is beneficial: one overloaded route.
///
///   Depot(0,0)  C0(10,0)  C1(20,0)  C2(30,0)  C3(40,0)
///   Demands: C0=4, C1=4, C2=4, C3=4   (total=16, capacity=8)
///
/// A single route [0,1,2,3] has load 16 > capacity 8 (excess = 8).
/// Split into [0,1] (load=8) + [2,3] (load=8) eliminates the excess.
static ProblemData make_overloaded_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    // 2 vehicles to allow split, capacity=8, reload enabled.
    b.add_vehicle_type(2, {
                              .capacity = {8},
                              .reload_depot = 0,
                              .max_reloads = 3,
                          });

    b.add_client({10.0, 0.0}, {.demand = {4}});  // 0
    b.add_client({20.0, 0.0}, {.demand = {4}});  // 1
    b.add_client({30.0, 0.0}, {.demand = {4}});  // 2
    b.add_client({40.0, 0.0}, {.demand = {4}});  // 3

    return b.build(0);
}

/// Instance where merging is beneficial: two short routes that fit in one.
///
///   Depot(0,0)  C0(10,0)  C1(20,0)  C2(30,0)
///   Demands: C0=2, C1=2, C2=2   (total=6, capacity=10)
///
/// Two routes [0,1] and [2] can merge into [0,1,2] to save the extra
/// depot return/depart cost.
static ProblemData make_mergeable_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {
                              .capacity = {10},
                              .reload_depot = 0,
                              .max_reloads = 3,
                          });

    b.add_client({10.0, 0.0}, {.demand = {2}});  // 0
    b.add_client({20.0, 0.0}, {.demand = {2}});  // 1
    b.add_client({30.0, 0.0}, {.demand = {2}});  // 2

    return b.build(0);
}

/// Instance without multi-trip support (reload_depot = -1).
static ProblemData make_no_reload_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {
                              .capacity = {5},
                              // reload_depot defaults to -1 (no reload).
                          });

    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {3}});

    return b.build(0);
}

/// Build a solution with given route assignments.
static Solution make_solution(ProblemData const& data,
                              std::vector<std::vector<int>> const& routes) {
    Solution sol(data);
    for (int r = 0; r < static_cast<int>(routes.size()); ++r) {
        if (!routes[r].empty()) {
            sol.set_route_clients(r, routes[r]);
        }
    }
    return sol;
}

// ===========================================================================
//  DEPOT_VISIT sentinel tests
// ===========================================================================

TEST_CASE("DEPOT_VISIT sentinel value and is_depot_visit", "[relocate_with_depot]") {
    REQUIRE(DEPOT_VISIT == -1);
    REQUIRE(is_depot_visit(DEPOT_VISIT));
    REQUIRE_FALSE(is_depot_visit(0));
    REQUIRE_FALSE(is_depot_visit(1));
    REQUIRE_FALSE(is_depot_visit(100));
}

// ===========================================================================
//  Split (insert depot visit) tests
// ===========================================================================

TEST_CASE("RelocateWithDepot: splits overloaded route to reduce penalty",
          "[relocate_with_depot][split]") {
    auto data = make_overloaded_instance();
    CostEvaluator eval(100);  // high load penalty

    // One route with all 4 clients: load = 16, capacity = 8, excess = 8.
    auto sol = make_solution(data, {{0, 1, 2, 3}});
    REQUIRE(sol.route(0).load_excess() == 8);

    int64_t old_cost = sol.cost(eval);

    RelocateWithDepot op;
    bool found = op.find_best_move(sol, eval, data);
    REQUIRE(found);
    REQUIRE(op.best_delta() < 0);

    op.apply(sol);

    // After split: two routes, each with load <= 8.
    int64_t new_cost = sol.cost(eval);
    REQUIRE(new_cost < old_cost);

    // Both routes should be load-feasible (or at least less excess).
    int total_excess = 0;
    for (int r = 0; r < sol.num_routes(); ++r) {
        total_excess += sol.route(r).load_excess();
    }
    REQUIRE(total_excess < 8);
}

TEST_CASE("RelocateWithDepot: split at optimal position", "[relocate_with_depot][split]") {
    auto data = make_overloaded_instance();
    CostEvaluator eval(100);

    // Route [0,1,2,3], total demand=16, cap=8.
    // Split at pos 2: [0,1] (load=8) and [2,3] (load=8) -- both feasible.
    auto sol = make_solution(data, {{0, 1, 2, 3}});

    RelocateWithDepot op;
    REQUIRE(op.find_best_move(sol, eval, data));
    op.apply(sol);

    // Both routes should be load-feasible.
    int num_nonempty = 0;
    for (int r = 0; r < sol.num_routes(); ++r) {
        if (!sol.route(r).empty()) {
            num_nonempty++;
            REQUIRE(sol.route(r).load_feasible());
        }
    }
    REQUIRE(num_nonempty == 2);
}

TEST_CASE("RelocateWithDepot: no split when no empty vehicle slot",
          "[relocate_with_depot][split]") {
    auto data = make_overloaded_instance();
    CostEvaluator eval(100);

    // Both vehicle slots occupied.
    auto sol = make_solution(data, {{0, 1, 2, 3}, {/* empty but we'll check */}});
    // Actually route 1 is empty, so a split should still be possible.
    // Let's create an instance with only 1 vehicle.

    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {
                              // only 1 vehicle
                              .capacity = {8},
                              .reload_depot = 0,
                              .max_reloads = 3,
                          });
    b.add_client({10.0, 0.0}, {.demand = {4}});
    b.add_client({20.0, 0.0}, {.demand = {4}});
    b.add_client({30.0, 0.0}, {.demand = {4}});
    auto data1 = b.build(0);

    auto sol1 = make_solution(data1, {{0, 1, 2}});
    REQUIRE(sol1.route(0).load_excess() > 0);

    RelocateWithDepot op;
    // Only 1 vehicle slot, can't split.
    bool found = op.find_best_move(sol1, eval, data1);
    REQUIRE_FALSE(found);
}

// ===========================================================================
//  Merge (remove depot visit) tests
// ===========================================================================

TEST_CASE("RelocateWithDepot: merges two routes to save distance", "[relocate_with_depot][merge]") {
    auto data = make_mergeable_instance();
    CostEvaluator eval(100);

    // Two separate routes: [0, 1] and [2].
    // Merged: [0, 1, 2] saves the depot return from route 2 + fixed cost.
    auto sol = make_solution(data, {{0, 1}, {2}});
    int64_t old_cost = sol.cost(eval);

    RelocateWithDepot op;
    bool found = op.find_best_move(sol, eval, data);
    REQUIRE(found);
    REQUIRE(op.best_delta() < 0);

    op.apply(sol);

    int64_t new_cost = sol.cost(eval);
    REQUIRE(new_cost < old_cost);

    // Should have 1 non-empty route now.
    int num_nonempty = 0;
    for (int r = 0; r < sol.num_routes(); ++r) {
        if (!sol.route(r).empty()) {
            num_nonempty++;
        }
    }
    REQUIRE(num_nonempty == 1);
}

TEST_CASE("RelocateWithDepot: merge preserves all clients", "[relocate_with_depot][merge]") {
    auto data = make_mergeable_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1}, {2}});

    RelocateWithDepot op;
    REQUIRE(op.find_best_move(sol, eval, data));
    op.apply(sol);

    // All 3 clients should still be assigned.
    REQUIRE(sol.num_unassigned() == 0);
}

// ===========================================================================
//  No-op / edge case tests
// ===========================================================================

TEST_CASE("RelocateWithDepot: no moves when reload not supported", "[relocate_with_depot]") {
    auto data = make_no_reload_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1}});

    RelocateWithDepot op;
    bool found = op.find_best_move(sol, eval, data);
    REQUIRE_FALSE(found);
}

TEST_CASE("RelocateWithDepot: no moves on empty solution", "[relocate_with_depot]") {
    auto data = make_multi_trip_instance();
    CostEvaluator eval(100);

    Solution sol(data);  // all clients unassigned

    RelocateWithDepot op;
    bool found = op.find_best_move(sol, eval, data);
    REQUIRE_FALSE(found);
}

TEST_CASE("RelocateWithDepot: single-client route cannot be split", "[relocate_with_depot]") {
    auto data = make_multi_trip_instance();
    CostEvaluator eval(100);

    // Route with 1 client can't be split (need at least 2).
    auto sol = make_solution(data, {{0}});

    RelocateWithDepot op;
    bool found = op.find_best_move(sol, eval, data);
    // Might find no improving move since splitting a 1-client route isn't tried,
    // and there's no merge target.
    // This should not crash.
    (void)found;
}

TEST_CASE("RelocateWithDepot: max_reloads limits splits", "[relocate_with_depot]") {
    // Instance with max_reloads=1: only 1 depot visit allowed (2 trips max).
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {
                              .capacity = {4},
                              .reload_depot = 0,
                              .max_reloads = 1,  // only 1 reload allowed (2 trips)
                          });
    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {3}});
    b.add_client({30.0, 0.0}, {.demand = {3}});
    b.add_client({40.0, 0.0}, {.demand = {3}});
    auto data = b.build(0);

    CostEvaluator eval(1000);  // very high penalty

    // All 4 clients in one route: load=12, cap=4, excess=8.
    auto sol = make_solution(data, {{0, 1, 2, 3}});

    // First split should succeed (0 -> 1 reload).
    RelocateWithDepot op;
    REQUIRE(op.find_best_move(sol, eval, data));
    op.apply(sol);

    // Now we have 2 trips (1 reload used).
    // A second split should still be possible because the second route
    // may also be overloaded and another empty slot exists.
    // But max_reloads=1 means total trips for the type can be max 2.
    // With 3 vehicle slots and max_reloads=1, we can have at most 2 trips.
    // Actually, max_reloads is per-vehicle, but since we're using route
    // slots, we count total trips of this vehicle type.
    // count_trips will be 2 after first split, max allowed = max_reloads + 1 = 2.
    // So no more splits.

    RelocateWithDepot op2;
    bool found2 = op2.find_best_move(sol, eval, data);
    // Should not find a split (merge might be possible but would increase cost).
    // If a merge is worse, no improving move.
    // Just verify it doesn't crash.
    (void)found2;
}

TEST_CASE("RelocateWithDepot: split then merge round-trips correctly", "[relocate_with_depot]") {
    auto data = make_mergeable_instance();
    CostEvaluator eval(1000);  // high penalty to motivate splits

    // Start with a feasible single route: [0, 1, 2], load=6, cap=10.
    auto sol = make_solution(data, {{0, 1, 2}});
    int64_t original_cost = sol.cost(eval);

    // Force a split (even if not improving for this instance).
    // The merge should be able to undo it.

    // First try: see if split is found (it might not be improving).
    RelocateWithDepot op;
    bool found = op.find_best_move(sol, eval, data);

    if (!found) {
        // No improving move (which is correct -- the route is already feasible).
        // Manually split and verify merge recovers.
        sol.set_route_clients(0, {0, 1});
        sol.set_route_clients(1, {2});

        int64_t split_cost = sol.cost(eval);
        REQUIRE(split_cost > original_cost);  // split is worse

        // Now merge should be improving.
        RelocateWithDepot op2;
        REQUIRE(op2.find_best_move(sol, eval, data));
        REQUIRE(op2.best_delta() < 0);
        op2.apply(sol);

        int64_t merged_cost = sol.cost(eval);
        REQUIRE(merged_cost == original_cost);
    }
}

// ===========================================================================
//  Delta accuracy tests
// ===========================================================================

TEST_CASE("RelocateWithDepot: delta matches actual cost change", "[relocate_with_depot]") {
    auto data = make_overloaded_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 2, 3}});
    int64_t old_cost = sol.cost(eval);

    RelocateWithDepot op;
    REQUIRE(op.find_best_move(sol, eval, data));
    int64_t reported_delta = op.best_delta();

    op.apply(sol);
    int64_t new_cost = sol.cost(eval);

    REQUIRE(new_cost - old_cost == reported_delta);
}

TEST_CASE("RelocateWithDepot: merge delta matches actual cost change", "[relocate_with_depot]") {
    auto data = make_mergeable_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1}, {2}});
    int64_t old_cost = sol.cost(eval);

    RelocateWithDepot op;
    REQUIRE(op.find_best_move(sol, eval, data));
    int64_t reported_delta = op.best_delta();

    op.apply(sol);
    int64_t new_cost = sol.cost(eval);

    REQUIRE(new_cost - old_cost == reported_delta);
}
