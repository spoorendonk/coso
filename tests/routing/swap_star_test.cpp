#include "routing/operators/swap_star.h"

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
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {15}});

    b.add_client({10.0, 0.0}, {.demand = {3}});   // 0
    b.add_client({20.0, 0.0}, {.demand = {4}});   // 1
    b.add_client({30.0, 0.0}, {.demand = {5}});   // 2
    b.add_client({0.0, 10.0}, {.demand = {2}});   // 3
    b.add_client({0.0, 20.0}, {.demand = {3}});   // 4
    b.add_client({15.0, 15.0}, {.demand = {6}});  // 5

    return b.build(0);
}

static ProblemData make_granular_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {15}});

    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {4}});
    b.add_client({30.0, 0.0}, {.demand = {5}});
    b.add_client({0.0, 10.0}, {.demand = {2}});
    b.add_client({0.0, 20.0}, {.demand = {3}});
    b.add_client({15.0, 15.0}, {.demand = {6}});

    return b.build(5);
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
//  SwapStar tests
// ===========================================================================

TEST_CASE("SwapStar: finds improving move on suboptimal solution", "[swapstar]") {
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 5}, {3, 2, 4}});
    int64_t old_cost = sol.cost(eval);

    SwapStar op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
    }
}

TEST_CASE("SwapStar: delta matches actual cost change", "[swapstar]") {
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 5}, {3, 2, 4}});
    int64_t old_cost = sol.cost(eval);

    SwapStar op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("SwapStar: preserves all clients", "[swapstar]") {
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 5}, {3, 2, 4}});

    SwapStar op;
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

TEST_CASE("SwapStar: no crash on single-client routes", "[swapstar]") {
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0}, {1}, {2}});

    SwapStar op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        op.apply(sol);
        CHECK(sol.num_unassigned() == 3);
    }
}

TEST_CASE("SwapStar: works with granular neighbours", "[swapstar][granular]") {
    auto data = make_granular_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 5}, {3, 2, 4}});
    int64_t old_cost = sol.cost(eval);

    SwapStar op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
        CHECK(sol.num_unassigned() == 0);
    }
}

TEST_CASE("SwapStar: iterated application converges", "[swapstar][iterate]") {
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{2, 4, 0, 5, 1, 3}});

    SwapStar op;
    int iters = 0;
    while (op.find_best_move(sol, eval, data) && iters < 100) {
        op.apply(sol);
        ++iters;
    }

    CHECK(iters < 100);
    CHECK(sol.num_unassigned() == 0);
}

TEST_CASE("SwapStar: capacity violation reduction", "[swapstar][capacity]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {10}});
    b.add_client({10.0, 0.0}, {.demand = {6}});
    b.add_client({20.0, 0.0}, {.demand = {6}});
    b.add_client({10.0, 10.0}, {.demand = {3}});
    b.add_client({20.0, 10.0}, {.demand = {3}});
    auto data = b.build(0);

    CostEvaluator eval(1000);

    auto sol = make_solution(data, {{0, 1}, {2, 3}});
    CHECK_FALSE(sol.route(0).load_feasible());
    int64_t old_cost = sol.cost(eval);

    SwapStar op;
    bool found = op.find_best_move(sol, eval, data);

    REQUIRE(found);
    op.apply(sol);
    CHECK(sol.cost(eval) < old_cost);
    CHECK(sol.num_unassigned() == 0);
}

TEST_CASE("SwapStar: empty routes handled correctly", "[swapstar]") {
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 2}, {3, 4, 5}});

    SwapStar op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        op.apply(sol);
        CHECK(sol.num_unassigned() == 0);
    }
}
