#include "routing/operators/route_split.h"

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace coso;

// ---------------------------------------------------------------------------
//  Test instance builders
// ---------------------------------------------------------------------------

static ProblemData make_test_instance() {
    // 3 vehicles, capacity 15 each.
    // Clients laid out to make splitting beneficial:
    // Depot at origin, clients in two clusters.
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {15}});

    b.add_client({10.0, 0.0}, {.demand = {3}});  // 0
    b.add_client({20.0, 0.0}, {.demand = {4}});  // 1
    b.add_client({30.0, 0.0}, {.demand = {5}});  // 2
    b.add_client({0.0, 10.0}, {.demand = {2}});  // 3
    b.add_client({0.0, 20.0}, {.demand = {3}});  // 4
    b.add_client({0.0, 30.0}, {.demand = {4}});  // 5

    return b.build(0);
}

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
//  RouteSplit tests
// ===========================================================================

TEST_CASE("RouteSplit: finds improving split on bad single route", "[route_split]") {
    // Put all 6 clients on one route with two clusters far apart.
    // The route goes east then north, doubling back through the depot.
    // Splitting should create two shorter routes, each in one cluster.
    auto data = make_test_instance();
    CostEvaluator eval(100);

    // All clients in one route, two empty vehicles available.
    auto sol = make_solution(data, {{0, 1, 2, 3, 4, 5}});
    int64_t old_cost = sol.cost(eval);

    RouteSplit op;
    bool found = op.find_best_move(sol, eval, data);

    REQUIRE(found);
    CHECK(op.best_delta() < 0);

    op.apply(sol);
    int64_t new_cost = sol.cost(eval);
    CHECK(new_cost < old_cost);
}

TEST_CASE("RouteSplit: delta matches actual cost change", "[route_split]") {
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 2, 3, 4, 5}});
    int64_t old_cost = sol.cost(eval);

    RouteSplit op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("RouteSplit: preserves all clients", "[route_split]") {
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 2, 3, 4, 5}});

    RouteSplit op;
    if (op.find_best_move(sol, eval, data)) {
        op.apply(sol);

        CHECK(sol.num_unassigned() == 0);
        std::vector<int> seen(data.num_clients(), 0);
        for (int r = 0; r < sol.num_routes(); ++r) {
            for (int i = 0; i < sol.route(r).size(); ++i) {
                seen[sol.route(r).client(i)]++;
            }
        }
        for (int c = 0; c < data.num_clients(); ++c) {
            CHECK(seen[c] == 1);
        }
    }
}

TEST_CASE("RouteSplit: no split when route has only 1 client", "[route_split]") {
    auto data = make_test_instance();
    CostEvaluator eval(100);

    // Three single-client routes: nothing to split.
    auto sol = make_solution(data, {{0}, {1}, {2}});

    RouteSplit op;
    CHECK_FALSE(op.find_best_move(sol, eval, data));
}

TEST_CASE("RouteSplit: no split when no empty vehicle available", "[route_split]") {
    // All 3 vehicles in use: no slot for the second half.
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {100}});

    b.add_client({10.0, 0.0}, {.demand = {1}});
    b.add_client({20.0, 0.0}, {.demand = {1}});
    b.add_client({0.0, 10.0}, {.demand = {1}});
    b.add_client({0.0, 20.0}, {.demand = {1}});
    b.add_client({10.0, 10.0}, {.demand = {1}});
    b.add_client({20.0, 10.0}, {.demand = {1}});

    auto data = b.build(0);
    CostEvaluator eval(100);

    // All 3 routes used.
    auto sol = make_solution(data, {{0, 1}, {2, 3}, {4, 5}});

    RouteSplit op;
    CHECK_FALSE(op.find_best_move(sol, eval, data));
}

TEST_CASE("RouteSplit: split reduces capacity violation", "[route_split]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {10}});

    // Two heavy clients: total demand 16 exceeds capacity 10.
    b.add_client({10.0, 0.0}, {.demand = {8}});
    b.add_client({20.0, 0.0}, {.demand = {8}});

    auto data = b.build(0);
    CostEvaluator eval(1000);  // high penalty

    // Both on one route: 6 units of excess.
    auto sol = make_solution(data, {{0, 1}});
    CHECK_FALSE(sol.route(0).load_feasible());
    int64_t old_cost = sol.cost(eval);

    RouteSplit op;
    bool found = op.find_best_move(sol, eval, data);

    REQUIRE(found);
    op.apply(sol);
    CHECK(sol.cost(eval) < old_cost);
    // Each route now has demand 8 <= capacity 10.
    CHECK(sol.feasible());
}

TEST_CASE("RouteSplit: two-client route splits into two singletons", "[route_split]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {5}});

    // Two heavy clients overloading a single route.
    b.add_client({10.0, 0.0}, {.demand = {4}});
    b.add_client({-10.0, 0.0}, {.demand = {4}});

    auto data = b.build(0);
    CostEvaluator eval(1000);

    auto sol = make_solution(data, {{0, 1}});
    int64_t old_cost = sol.cost(eval);

    RouteSplit op;
    bool found = op.find_best_move(sol, eval, data);

    REQUIRE(found);
    op.apply(sol);

    // Both clients assigned, no excess.
    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
    CHECK(sol.cost(eval) < old_cost);
}

TEST_CASE("RouteSplit: empty solution does not crash", "[route_split]") {
    auto data = make_test_instance();
    CostEvaluator eval(100);

    Solution sol(data);  // all unassigned

    RouteSplit op;
    CHECK_FALSE(op.find_best_move(sol, eval, data));
}

TEST_CASE("RouteSplit: iterated application converges", "[route_split]") {
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 2, 3, 4, 5}});

    RouteSplit op;
    int iters = 0;
    while (op.find_best_move(sol, eval, data) && iters < 100) {
        op.apply(sol);
        ++iters;
    }

    CHECK(iters < 100);
    CHECK(sol.num_unassigned() == 0);
}
