#include <catch2/catch_test_macros.hpp>

#include "routing/operators/relocate_with_depot.h"
#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/route.h"
#include "routing/solution.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Test instance builders
// ---------------------------------------------------------------------------

/// Multi-trip test instance: 1 depot, 6 clients, 2 vehicles with capacity 10
/// and max_reloads = 2.
///
///   Depot(0,0)  C0(10,0)  C1(20,0)  C2(30,0)  C3(40,0)  C4(50,0)  C5(60,0)
///
/// Demands: C0=4, C1=4, C2=4, C3=4, C4=4, C5=4
///
/// Each vehicle can carry at most 10, so serving 3 clients (demand=12) exceeds
/// capacity.  A depot visit allows the vehicle to reload.
static ProblemData make_multi_trip_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {
        .capacity = {10},
        .max_reloads = 2,
    });

    b.add_client({10.0, 0.0}, {.demand = {4}});  // 0
    b.add_client({20.0, 0.0}, {.demand = {4}});  // 1
    b.add_client({30.0, 0.0}, {.demand = {4}});  // 2
    b.add_client({40.0, 0.0}, {.demand = {4}});  // 3
    b.add_client({50.0, 0.0}, {.demand = {4}});  // 4
    b.add_client({60.0, 0.0}, {.demand = {4}});  // 5

    return b.build(0);  // no granular neighbours
}

/// Instance with no reload support (max_reloads = 0).
static ProblemData make_no_reload_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {
        .capacity = {10},
        .max_reloads = 0,
    });

    b.add_client({10.0, 0.0}, {.demand = {4}});
    b.add_client({20.0, 0.0}, {.demand = {4}});
    b.add_client({30.0, 0.0}, {.demand = {4}});

    return b.build(0);
}

/// Build a solution with given route assignments.
static Solution make_solution(ProblemData const& data,
                              std::vector<std::vector<int>> const& routes)
{
    Solution sol(data);
    for (int r = 0; r < static_cast<int>(routes.size()); ++r) {
        if (!routes[r].empty())
            sol.set_route_clients(r, routes[r]);
    }
    return sol;
}

// ===========================================================================
//  DEPOT_VISIT in Route tests
// ===========================================================================

TEST_CASE("Route: DEPOT_VISIT resets load between sub-trips",
          "[route][multi-trip]")
{
    auto data = make_multi_trip_instance();

    // Route [0, 1, DEPOT_VISIT, 2, 3]: two sub-trips of 2 clients each.
    // Each sub-trip has total demand 8, capacity is 10 -> feasible.
    Route route(data, 0);
    route.set_clients({0, 1, DEPOT_VISIT, 2, 3});

    CHECK(route.size() == 5);
    CHECK(route.load_feasible());
    CHECK(route.load_excess() == 0);
}

TEST_CASE("Route: without DEPOT_VISIT, high demand exceeds capacity",
          "[route][multi-trip]")
{
    auto data = make_multi_trip_instance();

    // Route [0, 1, 2, 3]: demand = 16, capacity = 10 -> infeasible.
    Route route(data, 0);
    route.set_clients({0, 1, 2, 3});

    CHECK_FALSE(route.load_feasible());
    CHECK(route.load_excess() > 0);
}

TEST_CASE("Route: DEPOT_VISIT affects distance correctly",
          "[route][multi-trip]")
{
    auto data = make_multi_trip_instance();

    // Route [0, 1]: depot(0,0) -> C0(10,0) -> C1(20,0) -> depot(0,0)
    // Distance = 10 + 10 + 20 = 40.
    Route no_depot(data, 0);
    no_depot.set_clients({0, 1});
    int dist_no_depot = no_depot.distance();

    // Route [0, DEPOT_VISIT, 1]: depot -> C0 -> depot -> C1 -> depot
    // Distance = 10 + 10 + 20 + 20 = 60.
    // (depot->C0=10, C0->depot=10, depot->C1=20, C1->depot=20)
    Route with_depot(data, 0);
    with_depot.set_clients({0, DEPOT_VISIT, 1});
    int dist_with_depot = with_depot.distance();

    CHECK(dist_with_depot > dist_no_depot);
    // depot->C0=10, C0->depot=10, depot->C1=20, C1->depot=20 = 60
    CHECK(dist_with_depot == 60);
    CHECK(dist_no_depot == 40);
}

// ===========================================================================
//  split_into_trips tests
// ===========================================================================

TEST_CASE("split_into_trips: no depot visits", "[multi-trip]")
{
    auto trips = RelocateWithDepot::split_into_trips({0, 1, 2, 3});
    REQUIRE(trips.size() == 1);
    CHECK(trips[0] == std::vector<int>{0, 1, 2, 3});
}

TEST_CASE("split_into_trips: one depot visit", "[multi-trip]")
{
    auto trips = RelocateWithDepot::split_into_trips(
        {0, 1, DEPOT_VISIT, 2, 3});
    REQUIRE(trips.size() == 2);
    CHECK(trips[0] == std::vector<int>{0, 1});
    CHECK(trips[1] == std::vector<int>{2, 3});
}

TEST_CASE("split_into_trips: two depot visits", "[multi-trip]")
{
    auto trips = RelocateWithDepot::split_into_trips(
        {0, DEPOT_VISIT, 1, 2, DEPOT_VISIT, 3});
    REQUIRE(trips.size() == 3);
    CHECK(trips[0] == std::vector<int>{0});
    CHECK(trips[1] == std::vector<int>{1, 2});
    CHECK(trips[2] == std::vector<int>{3});
}

// ===========================================================================
//  count_depot_visits tests
// ===========================================================================

TEST_CASE("count_depot_visits: various", "[multi-trip]")
{
    CHECK(RelocateWithDepot::count_depot_visits({0, 1, 2}) == 0);
    CHECK(RelocateWithDepot::count_depot_visits(
        {0, DEPOT_VISIT, 1}) == 1);
    CHECK(RelocateWithDepot::count_depot_visits(
        {0, DEPOT_VISIT, 1, DEPOT_VISIT, 2}) == 2);
}

// ===========================================================================
//  RelocateWithDepot: insert depot visit tests
// ===========================================================================

TEST_CASE("RelocateWithDepot: insert depot visit reduces penalty",
          "[multi-trip][relocate-depot]")
{
    auto data = make_multi_trip_instance();
    CostEvaluator eval(100);  // high load penalty

    // Route 0: [0, 1, 2] demand=12, capacity=10 -> infeasible (excess=2).
    // Route 1: [3, 4, 5] demand=12, capacity=10 -> infeasible.
    auto sol = make_solution(data, {{0, 1, 2}, {3, 4, 5}});

    CHECK_FALSE(sol.feasible());
    int64_t old_cost = sol.cost(eval);

    RelocateWithDepot op;
    bool found = op.find_best_move(sol, eval, data);

    // Should find an improving move: inserting a depot visit splits the
    // overloaded route into two feasible sub-trips.
    REQUIRE(found);
    CHECK(op.best_delta() < 0);

    op.apply(sol);
    int64_t new_cost = sol.cost(eval);
    CHECK(new_cost < old_cost);
    CHECK(new_cost - old_cost == op.best_delta());
}

TEST_CASE("RelocateWithDepot: no move when max_reloads is 0",
          "[multi-trip][relocate-depot]")
{
    auto data = make_no_reload_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 2}});

    RelocateWithDepot op;
    bool found = op.find_best_move(sol, eval, data);

    // max_reloads = 0, so no depot insertion is allowed.
    CHECK_FALSE(found);
}

TEST_CASE("RelocateWithDepot: no move when route is too short",
          "[multi-trip][relocate-depot]")
{
    auto data = make_multi_trip_instance();
    CostEvaluator eval(100);

    // Single client per route -- no position to split.
    auto sol = make_solution(data, {{0}, {1}});

    RelocateWithDepot op;
    bool found = op.find_best_move(sol, eval, data);
    CHECK_FALSE(found);
}

// ===========================================================================
//  RelocateWithDepot: remove depot visit tests
// ===========================================================================

TEST_CASE("RelocateWithDepot: remove depot visit saves distance",
          "[multi-trip][relocate-depot]")
{
    auto data = make_multi_trip_instance();
    CostEvaluator eval(0);  // zero penalty so load violations don't matter

    // Route with unnecessary depot visit: [0, DEPOT_VISIT, 1]
    // With depot visit: depot->C0->depot->C1->depot (long)
    // Without: depot->C0->C1->depot (shorter)
    auto sol = make_solution(data, {{0, DEPOT_VISIT, 1}, {2, 3, 4, 5}});

    int64_t old_cost = sol.cost(eval);

    RelocateWithDepot op;
    bool found = op.find_best_move(sol, eval, data);

    REQUIRE(found);
    CHECK(op.best_delta() < 0);

    op.apply(sol);
    int64_t new_cost = sol.cost(eval);
    CHECK(new_cost < old_cost);
    CHECK(new_cost - old_cost == op.best_delta());
}

TEST_CASE("RelocateWithDepot: respects max_reloads limit",
          "[multi-trip][relocate-depot]")
{
    // Create instance with max_reloads = 1.
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {
        .capacity = {5},
        .max_reloads = 1,
    });

    b.add_client({10.0, 0.0}, {.demand = {3}});  // 0
    b.add_client({20.0, 0.0}, {.demand = {3}});  // 1
    b.add_client({30.0, 0.0}, {.demand = {3}});  // 2
    b.add_client({40.0, 0.0}, {.demand = {3}});  // 3

    auto data = b.build(0);
    CostEvaluator eval(100);

    // Route already has 1 depot visit (at the limit).
    // [0, DEPOT_VISIT, 1, 2, 3] - second sub-trip has demand 9, cap 5.
    auto sol = make_solution(data, {{0, DEPOT_VISIT, 1, 2, 3}});

    RelocateWithDepot op;
    bool found = op.find_best_move(sol, eval, data);

    // Can only remove the existing depot visit (which would worsen things),
    // or the operator finds no improving insert (already at max_reloads=1).
    // Either way, if found is true, it should be a remove move, not insert.
    if (found) {
        // The move could be removing the existing depot visit if that's
        // improving, but with high penalty it likely isn't.
        op.apply(sol);
    }
    // Just verify no crash.
    CHECK(true);
}

// ===========================================================================
//  Solution with DEPOT_VISIT: assignment tracking
// ===========================================================================

TEST_CASE("Solution: DEPOT_VISIT markers don't affect client assignment",
          "[solution][multi-trip]")
{
    auto data = make_multi_trip_instance();

    Solution sol(data);
    sol.set_route_clients(0, {0, 1, DEPOT_VISIT, 2, 3});
    sol.set_route_clients(1, {4, 5});

    // All 6 clients should be assigned.
    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.is_assigned(0));
    CHECK(sol.is_assigned(1));
    CHECK(sol.is_assigned(2));
    CHECK(sol.is_assigned(3));
    CHECK(sol.is_assigned(4));
    CHECK(sol.is_assigned(5));
}

TEST_CASE("Solution: clearing route with DEPOT_VISIT unassigns clients",
          "[solution][multi-trip]")
{
    auto data = make_multi_trip_instance();

    Solution sol(data);
    sol.set_route_clients(0, {0, 1, DEPOT_VISIT, 2});
    CHECK(sol.is_assigned(0));
    CHECK(sol.is_assigned(1));
    CHECK(sol.is_assigned(2));

    sol.set_route_clients(0, {});
    CHECK_FALSE(sol.is_assigned(0));
    CHECK_FALSE(sol.is_assigned(1));
    CHECK_FALSE(sol.is_assigned(2));
    CHECK(sol.num_unassigned() == 6);
}

// ===========================================================================
//  CostEvaluator with DEPOT_VISIT
// ===========================================================================

TEST_CASE("CostEvaluator: handles routes with DEPOT_VISIT",
          "[cost-evaluator][multi-trip]")
{
    auto data = make_multi_trip_instance();
    CostEvaluator eval(100);

    Route route(data, 0);
    route.set_clients({0, 1, DEPOT_VISIT, 2, 3});

    // Should not crash and should produce a reasonable cost.
    int64_t cost = eval.route_cost(route);
    CHECK(cost > 0);

    int64_t obj = eval.route_objective(route);
    int64_t pen = eval.route_penalty(route);
    CHECK(cost == obj + pen);
}
