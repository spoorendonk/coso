#include "routing/operators/swap_star.h"

#include "routing/cost_evaluator.h"
#include "routing/local_search.h"
#include "routing/operators/exchange.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/stop_criterion.h"

#include <catch2/catch_test_macros.hpp>

#include <random>
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
    auto old_cost = sol.cost(eval);

    SwapStar op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        auto new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
    }
}

TEST_CASE("SwapStar: delta matches actual cost change", "[swapstar]") {
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 5}, {3, 2, 4}});
    auto old_cost = sol.cost(eval);

    SwapStar op;
    if (op.find_best_move(sol, eval, data)) {
        auto predicted = op.best_delta();
        op.apply(sol);
        auto actual = sol.cost(eval) - old_cost;
        CHECK(coso::costs_match(predicted, actual));
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
    auto old_cost = sol.cost(eval);

    SwapStar op;
    if (op.find_best_move(sol, eval, data)) {
        auto predicted = op.best_delta();
        op.apply(sol);
        auto actual = sol.cost(eval) - old_cost;
        CHECK(coso::costs_match(predicted, actual));
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
    auto old_cost = sol.cost(eval);

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

// ===========================================================================
//  SWAP* under a stop criterion (#250)
// ===========================================================================

// Two routes of three clients at capacity, a local optimum of Exchange10,
// Exchange11, Exchange20 and SwapTails that a SWAP* move improves (found by a
// random search over small instances).
static ProblemData make_swap_star_only_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {15}});
    b.add_client({-16.0, 4.0}, {.demand = {5}});   // 0
    b.add_client({-3.0, 7.0}, {.demand = {5}});    // 1
    b.add_client({19.0, -4.0}, {.demand = {5}});   // 2
    b.add_client({1.0, -12.0}, {.demand = {5}});   // 3
    b.add_client({8.0, -9.0}, {.demand = {5}});    // 4
    b.add_client({-8.0, -15.0}, {.demand = {5}});  // 5
    return b.build(0);
}

TEST_CASE("SwapStar: instance is a local optimum of the other operators", "[swapstar][stop]") {
    auto data = make_swap_star_only_instance();
    CostEvaluator eval(1000);
    auto sol = make_solution(data, {{0, 1, 2}, {4, 3, 5}});

    Exchange10 op10;
    Exchange11 op11;
    Exchange20 op20;
    SwapTails op_st;
    CHECK_FALSE(op10.find_best_move(sol, eval, data));
    CHECK_FALSE(op11.find_best_move(sol, eval, data));
    CHECK_FALSE(op20.find_best_move(sol, eval, data));
    CHECK_FALSE(op_st.find_best_move(sol, eval, data));

    SwapStar op;
    REQUIRE(op.find_best_move(sol, eval, data));
    CHECK(op.best_delta() < 0);
}

TEST_CASE("LocalSearch: SWAP* is applied in a stop-bounded run", "[swapstar][stop]") {
    auto data = make_swap_star_only_instance();
    CostEvaluator eval(1000);
    auto sol = make_solution(data, {{0, 1, 2}, {4, 3, 5}});
    auto old_cost = sol.cost(eval);

    StopCriterion stop(60.0, 0, 0);
    LocalSearch ls(data);
    ls.run(sol, eval, &stop);

    CHECK(ls.last_num_swap_star_moves() >= 1);
    CHECK(sol.cost(eval) < old_cost);
    CHECK(sol.num_unassigned() == 0);
}

TEST_CASE("SwapStar: a stop that has fired ends the scan", "[swapstar][stop]") {
    auto data = make_swap_star_only_instance();
    CostEvaluator eval(1000);
    auto sol = make_solution(data, {{0, 1, 2}, {4, 3, 5}});

    StopCriterion stop(0.0, 1, 0);
    stop.iteration();
    REQUIRE(stop.should_stop());

    SwapStar op;
    CHECK_FALSE(op.find_best_move(sol, eval, data, &stop));
}

TEST_CASE("SwapStar: delta is exact on random instances", "[swapstar][random]") {
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> coord(-30, 30);
    std::uniform_int_distribution<int> demand(1, 8);

    int moves = 0;
    for (int trial = 0; trial < 200; ++trial) {
        ProblemData::Builder b;
        b.add_depot({0.0, 0.0});
        b.add_vehicle_type(3, {.capacity = {20}});
        for (int i = 0; i < 9; ++i) {
            b.add_client({double(coord(rng)), double(coord(rng))}, {.demand = {demand(rng)}});
        }
        auto data = b.build(0);
        CostEvaluator eval(50);
        auto sol = make_solution(data, {{0, 1, 2}, {3, 4, 5, 6}, {7, 8}});

        SwapStar op;
        for (int it = 0; it < 20 && op.find_best_move(sol, eval, data); ++it) {
            auto before = sol.cost(eval);
            auto predicted = op.best_delta();
            op.apply(sol);
            REQUIRE(coso::costs_match(sol.cost(eval) - before, predicted));
            REQUIRE(sol.num_unassigned() == 0);
            ++moves;
        }
    }
    CHECK(moves > 0);
}
