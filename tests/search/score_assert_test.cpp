#include <catch2/catch_test_macros.hpp>

#include "routing/cost_evaluator.h"
#include "routing/solution.h"
#include "search/score_assert.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small CVRP instance for testing.
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), 4 clients, 1 vehicle type with 3 vehicles, capacity 15.
static ProblemData make_test_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {15}});

    b.add_client({10.0, 0.0}, {.demand = {3}});   // client 0
    b.add_client({20.0, 0.0}, {.demand = {4}});   // client 1
    b.add_client({30.0, 0.0}, {.demand = {5}});   // client 2
    b.add_client({0.0, 10.0}, {.demand = {2}});   // client 3

    return b.build(0);
}

// ===========================================================================
//  assert_route_consistency
// ===========================================================================

TEST_CASE("assert_route_consistency: empty route passes",
          "[score_assert]")
{
    auto data = make_test_instance();
    Route route(data, 0);

    REQUIRE_NOTHROW(debug::assert_route_consistency(route, data));
}

TEST_CASE("assert_route_consistency: single-client route passes",
          "[score_assert]")
{
    auto data = make_test_instance();
    Route route(data, 0);
    route.insert(0, 0);

    REQUIRE_NOTHROW(debug::assert_route_consistency(route, data));
}

TEST_CASE("assert_route_consistency: multi-client route passes",
          "[score_assert]")
{
    auto data = make_test_instance();
    Route route(data, 0);
    route.set_clients({0, 1, 2, 3});

    REQUIRE_NOTHROW(debug::assert_route_consistency(route, data));
}

TEST_CASE("assert_route_consistency: after insert/remove",
          "[score_assert]")
{
    auto data = make_test_instance();
    Route route(data, 0);

    route.insert(0, 2);
    REQUIRE_NOTHROW(debug::assert_route_consistency(route, data));

    route.insert(0, 0);
    REQUIRE_NOTHROW(debug::assert_route_consistency(route, data));

    route.insert(1, 1);  // between 0 and 2
    REQUIRE_NOTHROW(debug::assert_route_consistency(route, data));

    route.remove(1);  // remove client 1
    REQUIRE_NOTHROW(debug::assert_route_consistency(route, data));

    route.replace(0, 3);  // replace client 0 with client 3
    REQUIRE_NOTHROW(debug::assert_route_consistency(route, data));
}

// ===========================================================================
//  assert_solution_consistency
// ===========================================================================

TEST_CASE("assert_solution_consistency: empty solution passes",
          "[score_assert]")
{
    auto data = make_test_instance();
    Solution sol(data);
    CostEvaluator eval(100);

    REQUIRE_NOTHROW(
        debug::assert_solution_consistency(sol, eval, data));
}

TEST_CASE("assert_solution_consistency: fully assigned solution passes",
          "[score_assert]")
{
    auto data = make_test_instance();
    Solution sol(data);
    CostEvaluator eval(100);

    sol.set_route_clients(0, {0, 1});
    sol.set_route_clients(1, {2, 3});

    REQUIRE_NOTHROW(
        debug::assert_solution_consistency(sol, eval, data));
}

TEST_CASE("assert_solution_consistency: partially assigned solution passes",
          "[score_assert]")
{
    auto data = make_test_instance();
    Solution sol(data);
    CostEvaluator eval(100);

    sol.set_route_clients(0, {0, 2});
    // clients 1 and 3 remain unassigned

    CHECK(sol.num_unassigned() == 2);
    REQUIRE_NOTHROW(
        debug::assert_solution_consistency(sol, eval, data));
}

TEST_CASE("assert_solution_consistency: after modifications",
          "[score_assert]")
{
    auto data = make_test_instance();
    Solution sol(data);
    CostEvaluator eval(50);

    sol.set_route_clients(0, {0, 1, 2, 3});
    REQUIRE_NOTHROW(
        debug::assert_solution_consistency(sol, eval, data));

    sol.remove_client(0, 2);  // remove client 2
    REQUIRE_NOTHROW(
        debug::assert_solution_consistency(sol, eval, data));

    sol.insert_client(1, 0, 2);  // insert client 2 into route 1
    REQUIRE_NOTHROW(
        debug::assert_solution_consistency(sol, eval, data));
}

// ===========================================================================
//  assert_cost_delta
// ===========================================================================

TEST_CASE("assert_cost_delta: matching deltas pass", "[score_assert]")
{
    REQUIRE_NOTHROW(debug::assert_cost_delta(42, 42));
    REQUIRE_NOTHROW(debug::assert_cost_delta(-10, -10));
    REQUIRE_NOTHROW(debug::assert_cost_delta(0, 0));
    REQUIRE_NOTHROW(debug::assert_cost_delta(0, 0, "insert at pos 3"));
}

TEST_CASE("assert_cost_delta: validates insert delta evaluation",
          "[score_assert]")
{
    auto data = make_test_instance();
    Solution sol(data);
    CostEvaluator eval(100);

    sol.set_route_clients(0, {0, 1});

    // Predict cost delta for inserting client 2 at position 1.
    int64_t cost_before = sol.cost(eval);
    int64_t predicted = eval.eval_insert_cost(sol.route(0), 1, 2);

    sol.insert_client(0, 1, 2);
    int64_t cost_after = sol.cost(eval);
    int64_t actual = cost_after - cost_before;

    REQUIRE_NOTHROW(debug::assert_cost_delta(predicted, actual,
                                              "insert client 2 at pos 1"));
}

TEST_CASE("assert_cost_delta: validates remove delta evaluation",
          "[score_assert]")
{
    auto data = make_test_instance();
    Solution sol(data);
    CostEvaluator eval(100);

    sol.set_route_clients(0, {0, 1, 2});

    // Predict cost delta for removing client at position 1.
    int64_t cost_before = sol.cost(eval);
    int64_t predicted = eval.eval_remove_cost(sol.route(0), 1);

    sol.remove_client(0, 1);
    int64_t cost_after = sol.cost(eval);
    int64_t actual = cost_after - cost_before;

    REQUIRE_NOTHROW(debug::assert_cost_delta(predicted, actual,
                                              "remove client at pos 1"));
}

// ===========================================================================
//  Overloaded route (feasibility check through assertions)
// ===========================================================================

TEST_CASE("assert_solution_consistency: overloaded route passes",
          "[score_assert]")
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {5}});

    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {4}});

    auto data = b.build(0);
    Solution sol(data);
    CostEvaluator eval(200);

    // Total demand = 7 > capacity 5 -> overloaded but consistency still holds.
    sol.set_route_clients(0, {0, 1});

    CHECK_FALSE(sol.feasible());
    REQUIRE_NOTHROW(
        debug::assert_solution_consistency(sol, eval, data));
}
