#include "routing/solution.h"

#include "routing/cost_evaluator.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small CVRP instance for testing.
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), 4 clients, 2 vehicle types (2 vehicles each = 4 total).
/// Vehicle type 0: capacity 10, unit_distance_cost 1, fixed_cost 0.
/// Vehicle type 1: capacity 15, unit_distance_cost 1, fixed_cost 20.
static ProblemData make_solution_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {10}});
    b.add_vehicle_type(2, {.capacity = {15}, .cost = {.fixed_cost = 20}});

    b.add_client({10.0, 0.0}, {.demand = {3}});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {4}});  // client 1
    b.add_client({30.0, 0.0}, {.demand = {5}});  // client 2
    b.add_client({0.0, 10.0}, {.demand = {2}});  // client 3

    return b.build(0);
}

/// Instance with optional clients (prizes).
static ProblemData make_prize_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {3}, .prize = 50, .required = false});  // 0
    b.add_client({20.0, 0.0}, {.demand = {4}, .prize = 30, .required = false});  // 1
    b.add_client({30.0, 0.0}, {.demand = {5}});                                  // 2

    return b.build(0);
}

// ===========================================================================
//  Solution construction tests
// ===========================================================================

TEST_CASE("Solution: initial state has all clients unassigned", "[solution]") {
    auto data = make_solution_instance();
    Solution sol(data);

    CHECK(sol.num_routes() == 4);  // 2 + 2 vehicles
    CHECK(sol.num_unassigned() == 4);
    CHECK(sol.num_used_vehicles() == 0);

    for (int v = 0; v < sol.num_routes(); ++v) {
        CHECK(sol.route(v).empty());
    }

    for (int c = 0; c < data.num_clients(); ++c) {
        CHECK_FALSE(sol.is_assigned(c));
    }
}

TEST_CASE("Solution: set_route_clients assigns and tracks correctly", "[solution]") {
    auto data = make_solution_instance();
    Solution sol(data);

    sol.set_route_clients(0, {0, 1});  // vehicle 0 serves clients 0, 1
    sol.set_route_clients(2, {2, 3});  // vehicle 2 serves clients 2, 3

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.num_used_vehicles() == 2);
    CHECK(sol.is_assigned(0));
    CHECK(sol.is_assigned(1));
    CHECK(sol.is_assigned(2));
    CHECK(sol.is_assigned(3));

    CHECK(sol.route(0).size() == 2);
    CHECK(sol.route(0).client(0) == 0);
    CHECK(sol.route(0).client(1) == 1);
    CHECK(sol.route(2).size() == 2);
}

TEST_CASE("Solution: set_route_clients can reassign clients", "[solution]") {
    auto data = make_solution_instance();
    Solution sol(data);

    sol.set_route_clients(0, {0, 1, 2, 3});
    CHECK(sol.num_unassigned() == 0);

    // Move client 2 and 3 to a different route.
    sol.set_route_clients(0, {0, 1});
    CHECK(sol.num_unassigned() == 2);
    CHECK_FALSE(sol.is_assigned(2));
    CHECK_FALSE(sol.is_assigned(3));

    sol.set_route_clients(1, {2, 3});
    CHECK(sol.num_unassigned() == 0);
}

TEST_CASE("Solution: insert_client and remove_client", "[solution]") {
    auto data = make_solution_instance();
    Solution sol(data);

    // Insert clients one by one.
    sol.insert_client(0, 0, 0);
    CHECK(sol.is_assigned(0));
    CHECK(sol.num_unassigned() == 3);

    sol.insert_client(0, 1, 1);
    CHECK(sol.route(0).size() == 2);
    CHECK(sol.num_unassigned() == 2);

    // Remove client 0 (at position 0).
    sol.remove_client(0, 0);
    CHECK_FALSE(sol.is_assigned(0));
    CHECK(sol.num_unassigned() == 3);
    CHECK(sol.route(0).size() == 1);
    CHECK(sol.route(0).client(0) == 1);  // client 1 remains
}

// ===========================================================================
//  Cost evaluation with Solution
// ===========================================================================

TEST_CASE("Solution: cost sums across all routes", "[solution]") {
    auto data = make_solution_instance();
    Solution sol(data);
    CostEvaluator eval(100);  // load_penalty = 100

    // Empty solution has zero cost.
    CHECK(sol.cost(eval) == 0);
    CHECK(sol.objective(eval) == 0);
    CHECK(sol.penalty(eval) == 0);

    // Assign clients to routes.
    // Route 0 (vtype 0, cap=10): clients 0(3), 1(4) -> load=7, feasible.
    // Distance: depot(0,0)->c0(10,0)->c1(20,0)->depot(0,0) = 10+10+20 = 40.
    sol.set_route_clients(0, {0, 1});

    // Route 2 (vtype 1, cap=15, fixed=20): clients 2(5), 3(2) -> load=7, feasible.
    sol.set_route_clients(2, {2, 3});

    CHECK(sol.feasible());

    // Route 0 objective: 40 * 1 + 0 (fixed) = 40.
    // Route 2 objective: distance + 20 fixed.
    int64_t r0_obj = eval.route_objective(sol.route(0));
    int64_t r2_obj = eval.route_objective(sol.route(2));

    CHECK(r0_obj == 40);
    CHECK(sol.objective(eval) == r0_obj + r2_obj);
    CHECK(sol.penalty(eval) == 0);
    CHECK(sol.cost(eval) == sol.objective(eval));
}

TEST_CASE("Solution: penalty for overloaded route", "[solution]") {
    auto data = make_solution_instance();
    Solution sol(data);
    CostEvaluator eval(100);

    // Route 0 (cap=10): clients 0(3), 1(4), 2(5) -> load=12, excess=2.
    sol.set_route_clients(0, {0, 1, 2});

    CHECK_FALSE(sol.feasible());
    CHECK(sol.penalty(eval) == 200);  // 2 * 100
}

TEST_CASE("Solution: total_distance across routes", "[solution]") {
    auto data = make_solution_instance();
    Solution sol(data);

    sol.set_route_clients(0, {0, 1});
    sol.set_route_clients(1, {2});

    int expected = sol.route(0).distance() + sol.route(1).distance();
    CHECK(sol.total_distance() == expected);
}

// ===========================================================================
//  Copy semantics
// ===========================================================================

TEST_CASE("Solution: copy creates independent solution", "[solution]") {
    auto data = make_solution_instance();
    Solution sol(data);
    sol.set_route_clients(0, {0, 1});

    // Copy.
    Solution copy = sol;

    CHECK(copy.num_routes() == sol.num_routes());
    CHECK(copy.route(0).size() == 2);
    CHECK(copy.num_unassigned() == sol.num_unassigned());

    // Modify copy, original unchanged.
    copy.remove_client(0, 0);
    CHECK(copy.route(0).size() == 1);
    CHECK(sol.route(0).size() == 2);  // original unchanged
}

// ===========================================================================
//  Prize handling through Solution
// ===========================================================================

TEST_CASE("Solution: prizes reduce objective cost", "[solution]") {
    auto data = make_prize_instance();
    Solution sol(data);
    CostEvaluator eval(100);

    // Serve client 0 (prize=50).
    sol.set_route_clients(0, {0});
    int64_t obj_with_prize = sol.objective(eval);

    // The distance cost is depot->c0->depot = 20.
    // Objective = 20 - 50 = -30 (prize exceeds travel cost).
    CHECK(obj_with_prize == -30);
}
