#include "search/score_analysis.h"

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small CVRP instance (4 clients, 1 depot, 2 vehicles cap 20)
// ---------------------------------------------------------------------------

static ProblemData make_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

    b.add_client({10.0, 0.0}, {.demand = {8}});
    b.add_client({0.0, 10.0}, {.demand = {6}});
    b.add_client({-10.0, 0.0}, {.demand = {7}});
    b.add_client({0.0, -10.0}, {.demand = {9}});

    b.add_vehicle_type(2, {.capacity = {20}});

    return b.build();
}

TEST_CASE("analyze — all clients assigned across two routes", "[score_analysis]") {
    auto data = make_instance();
    CostEvaluator eval;
    Solution sol(data);

    // Route 0: clients 0, 1
    sol.set_route_clients(0, {0, 1});
    // Route 1: clients 2, 3
    sol.set_route_clients(1, {2, 3});

    auto analysis = analyze(sol, eval, data);

    CHECK(analysis.num_routes_used == 2);
    CHECK(analysis.num_unserved == 0);
    CHECK(analysis.feasible);
    CHECK(analysis.routes.size() == 2);

    // Penalized cost should equal objective when feasible.
    CHECK(analysis.penalized_cost == analysis.total_objective + analysis.total_penalty);

    // Check total objective matches Solution::objective.
    CHECK(analysis.total_objective == sol.objective(eval));
    CHECK(analysis.total_penalty == sol.penalty(eval));
    CHECK(analysis.penalized_cost == sol.cost(eval));
}

TEST_CASE("analyze — route demand and capacity breakdown", "[score_analysis]") {
    auto data = make_instance();
    CostEvaluator eval;
    Solution sol(data);

    // Route 0: clients 0, 1 -> demand 8 + 6 = 14
    sol.set_route_clients(0, {0, 1});
    // Route 1: clients 2, 3 -> demand 7 + 9 = 16
    sol.set_route_clients(1, {2, 3});

    auto analysis = analyze(sol, eval, data);

    REQUIRE(analysis.routes.size() == 2);

    // First route: demand should be 14, capacity 20.
    auto const& r0 = analysis.routes[0];
    REQUIRE(r0.total_demand.size() == 1);
    CHECK(r0.total_demand[0] == 14);
    CHECK(r0.capacity[0] == 20);
    CHECK(r0.load_excess == 0);
    CHECK(r0.clients.size() == 2);

    // Second route: demand should be 16, capacity 20.
    auto const& r1 = analysis.routes[1];
    REQUIRE(r1.total_demand.size() == 1);
    CHECK(r1.total_demand[0] == 16);
    CHECK(r1.capacity[0] == 20);
    CHECK(r1.load_excess == 0);
}

TEST_CASE("analyze — unserved clients", "[score_analysis]") {
    auto data = make_instance();
    CostEvaluator eval;
    Solution sol(data);

    // Only assign 2 of 4 clients.
    sol.set_route_clients(0, {0, 1});

    auto analysis = analyze(sol, eval, data);

    CHECK(analysis.num_routes_used == 1);
    CHECK(analysis.num_unserved == 2);
    CHECK(analysis.routes.size() == 1);
}

TEST_CASE("analyze — infeasible solution with load excess", "[score_analysis]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

    // Three clients with total demand 30, vehicle capacity 15.
    b.add_client({10.0, 0.0}, {.demand = {10}});
    b.add_client({0.0, 10.0}, {.demand = {10}});
    b.add_client({-10.0, 0.0}, {.demand = {10}});

    b.add_vehicle_type(1, {.capacity = {15}});
    auto data = b.build();

    CostEvaluator eval(50);  // load_penalty = 50
    Solution sol(data);
    sol.set_route_clients(0, {0, 1, 2});

    auto analysis = analyze(sol, eval, data);

    CHECK_FALSE(analysis.feasible);
    CHECK(analysis.num_routes_used == 1);
    CHECK(analysis.total_penalty > 0);
    CHECK(analysis.penalized_cost > analysis.total_objective);

    auto const& r = analysis.routes[0];
    CHECK(r.load_excess > 0);
    CHECK(r.total_demand[0] == 30);
    CHECK(r.capacity[0] == 15);
}

TEST_CASE("analyze — empty solution", "[score_analysis]") {
    auto data = make_instance();
    CostEvaluator eval;
    Solution sol(data);

    auto analysis = analyze(sol, eval, data);

    CHECK(analysis.num_routes_used == 0);
    CHECK(analysis.num_unserved == 4);
    CHECK(analysis.feasible);
    CHECK(analysis.total_objective == 0);
    CHECK(analysis.total_penalty == 0);
    CHECK(analysis.penalized_cost == 0);
    CHECK(analysis.routes.empty());
}

TEST_CASE("analyze — to_string produces output", "[score_analysis]") {
    auto data = make_instance();
    CostEvaluator eval;
    Solution sol(data);

    sol.set_route_clients(0, {0, 1});
    sol.set_route_clients(1, {2, 3});

    auto analysis = analyze(sol, eval, data);
    std::string str = analysis.to_string();

    // Should contain key labels.
    CHECK(str.find("Solution Analysis") != std::string::npos);
    CHECK(str.find("Routes used") != std::string::npos);
    CHECK(str.find("Objective") != std::string::npos);
    CHECK(str.find("Penalty") != std::string::npos);
    CHECK(str.find("Clients") != std::string::npos);
    CHECK(str.find("Dim 0") != std::string::npos);
}

TEST_CASE("analyze — fixed cost included in objective", "[score_analysis]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_client({10.0, 0.0}, {.demand = {5}});
    b.add_vehicle_type(1, {.capacity = {10}, .cost = {.fixed_cost = 500}});
    auto data = b.build();

    CostEvaluator eval;
    Solution sol(data);
    sol.set_route_clients(0, {0});

    auto analysis = analyze(sol, eval, data);

    REQUIRE(analysis.routes.size() == 1);
    CHECK(analysis.routes[0].fixed_cost == 500);
    CHECK(analysis.total_objective == sol.objective(eval));
}
