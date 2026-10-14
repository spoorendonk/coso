#include "routing/cost_evaluator.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build test instances.
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), 4 clients on x-axis, capacity 10.
static ProblemData make_basic_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {3}});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {4}});  // client 1
    b.add_client({30.0, 0.0}, {.demand = {5}});  // client 2
    b.add_client({0.0, 10.0}, {.demand = {2}});  // client 3

    return b.build(0);
}

/// Instance with prizes.
static ProblemData make_prize_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {3}, .prize = 50});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {4}, .prize = 30});  // client 1

    return b.build(0);
}

// ===========================================================================
//  CostEvaluator construction
// ===========================================================================

TEST_CASE("CostEvaluator: default construction", "[cost_evaluator]") {
    CostEvaluator eval;
    CHECK(eval.load_penalty() == 100);
    CHECK(eval.tw_penalty() == 100);
    CHECK(eval.dist_penalty() == 100);
}

TEST_CASE("CostEvaluator: custom penalties", "[cost_evaluator]") {
    CostEvaluator eval(200, 50, 75);
    CHECK(eval.load_penalty() == 200);
    CHECK(eval.tw_penalty() == 50);
    CHECK(eval.dist_penalty() == 75);
}

TEST_CASE("CostEvaluator: penalty setters", "[cost_evaluator]") {
    CostEvaluator eval;
    eval.set_load_penalty(500);
    eval.set_tw_penalty(300);
    eval.set_dist_penalty(150);

    CHECK(eval.load_penalty() == 500);
    CHECK(eval.tw_penalty() == 300);
    CHECK(eval.dist_penalty() == 150);
}

// ===========================================================================
//  Route objective
// ===========================================================================

TEST_CASE("CostEvaluator: empty route has zero cost", "[cost_evaluator]") {
    auto data = make_basic_instance();
    Route route(data, 0);
    CostEvaluator eval;

    CHECK(eval.route_objective(route) == 0);
    CHECK(eval.route_penalty(route) == 0);
    CHECK(eval.route_cost(route) == 0);
}

TEST_CASE("CostEvaluator: route_objective with distance cost", "[cost_evaluator]") {
    auto data = make_basic_instance();
    Route route(data, 0);
    CostEvaluator eval;

    // depot(0,0) -> c0(10,0) -> c1(20,0) -> depot(0,0)
    // Distance: 10 + 10 + 20 = 40.
    route.set_clients({0, 1});
    CHECK(eval.route_objective(route) == 40);
}

TEST_CASE("CostEvaluator: route_objective subtracts prizes", "[cost_evaluator]") {
    auto data = make_prize_instance();
    Route route(data, 0);
    CostEvaluator eval;

    route.set_clients({0});
    // Distance: 10 + 10 = 20.  Objective: 20 - 50 = -30.
    CHECK(eval.route_objective(route) == -30);

    route.set_clients({0, 1});
    // Distance: 40.  Prizes: 50 + 30 = 80.  Objective: 40 - 80 = -40.
    CHECK(eval.route_objective(route) == -40);
}

// ===========================================================================
//  Route penalty
// ===========================================================================

TEST_CASE("CostEvaluator: no penalty when feasible", "[cost_evaluator]") {
    auto data = make_basic_instance();
    Route route(data, 0);
    CostEvaluator eval(100);

    route.set_clients({0, 1});  // load=7, cap=10
    CHECK(eval.route_penalty(route) == 0);
}

TEST_CASE("CostEvaluator: load penalty for overloaded route", "[cost_evaluator]") {
    auto data = make_basic_instance();
    Route route(data, 0);
    CostEvaluator eval(100);

    route.set_clients({0, 1, 2});  // load=12, cap=10, excess=2
    CHECK(eval.route_penalty(route) == 200);
}

TEST_CASE("CostEvaluator: penalty weight affects penalty", "[cost_evaluator]") {
    auto data = make_basic_instance();
    Route route(data, 0);
    route.set_clients({0, 1, 2});  // excess=2

    CostEvaluator eval1(50);
    CHECK(eval1.route_penalty(route) == 100);

    CostEvaluator eval2(200);
    CHECK(eval2.route_penalty(route) == 400);
}

TEST_CASE("CostEvaluator: route_cost = objective + penalty", "[cost_evaluator]") {
    auto data = make_basic_instance();
    Route route(data, 0);
    CostEvaluator eval(100);

    route.set_clients({0, 1, 2});  // excess=2
    int64_t obj = eval.route_objective(route);
    int64_t pen = eval.route_penalty(route);
    CHECK(eval.route_cost(route) == obj + pen);
}

// ===========================================================================
//  Delta evaluation: insert
// ===========================================================================

TEST_CASE("CostEvaluator: eval_insert_cost matches actual cost change", "[cost_evaluator]") {
    auto data = make_basic_instance();
    CostEvaluator eval(100);

    Route route(data, 0);
    route.set_clients({0, 1});
    int64_t old_cost = eval.route_cost(route);

    // Test inserting client 2 at every position.
    for (int pos = 0; pos <= route.size(); ++pos) {
        int64_t delta = eval.eval_insert_cost(route, pos, 2);

        // Actually insert and compare.
        Route copy(data, 0);
        copy.set_clients({0, 1});
        copy.insert(pos, 2);
        int64_t new_cost = eval.route_cost(copy);

        CHECK(delta == new_cost - old_cost);
    }
}

TEST_CASE("CostEvaluator: eval_insert_cost accounts for prizes", "[cost_evaluator]") {
    auto data = make_prize_instance();
    CostEvaluator eval(100);

    Route route(data, 0);
    int64_t delta = eval.eval_insert_cost(route, 0, 0);

    // Inserting client 0 (prize=50): distance=20, prize=-50.
    // Delta = 20 - 50 = -30.
    CHECK(delta == -30);
}

// ===========================================================================
//  Delta evaluation: remove
// ===========================================================================

TEST_CASE("CostEvaluator: eval_remove_cost matches actual cost change", "[cost_evaluator]") {
    auto data = make_basic_instance();
    CostEvaluator eval(100);

    Route route(data, 0);
    route.set_clients({0, 1, 2});
    int64_t old_cost = eval.route_cost(route);

    for (int pos = 0; pos < route.size(); ++pos) {
        int64_t delta = eval.eval_remove_cost(route, pos);

        Route copy(data, 0);
        copy.set_clients({0, 1, 2});
        copy.remove(pos);
        int64_t new_cost = eval.route_cost(copy);

        CHECK(delta == new_cost - old_cost);
    }
}

TEST_CASE("CostEvaluator: eval_remove_cost accounts for prizes", "[cost_evaluator]") {
    auto data = make_prize_instance();
    CostEvaluator eval(100);

    Route route(data, 0);
    route.set_clients({0, 1});
    int64_t old_cost = eval.route_cost(route);

    // Remove client 0 (prize=50).
    int64_t delta = eval.eval_remove_cost(route, 0);

    Route copy(data, 0);
    copy.set_clients({1});
    int64_t new_cost = eval.route_cost(copy);

    CHECK(delta == new_cost - old_cost);
}

// ===========================================================================
//  Delta evaluation consistency: insert then remove = identity
// ===========================================================================

TEST_CASE("CostEvaluator: insert + remove delta sums to zero", "[cost_evaluator]") {
    auto data = make_basic_instance();
    CostEvaluator eval(100);

    Route route(data, 0);
    route.set_clients({0, 1});

    // Insert client 3 at position 1.
    int64_t insert_delta = eval.eval_insert_cost(route, 1, 3);

    // Actually insert.
    route.insert(1, 3);

    // Remove client 3 (now at position 1).
    int64_t remove_delta = eval.eval_remove_cost(route, 1);

    // Net delta should be zero.
    CHECK(insert_delta + remove_delta == 0);
}

// ===========================================================================
//  Splice deltas price exactly what route_cost prices (#221)
// ===========================================================================

/// Tight time windows, service times, prizes and a small capacity, so a
/// splice moves every term route_cost charges.
static ProblemData make_tw_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {8}});

    b.add_client({10.0, 0.0}, {.demand = {3}, .tw = {0, 15}, .service = 5, .prize = 7});
    b.add_client({20.0, 0.0}, {.demand = {4}, .tw = {10, 30}, .service = 5});
    b.add_client({30.0, 0.0}, {.demand = {2}, .tw = {20, 40}, .service = 5, .prize = 4});
    b.add_client({0.0, 10.0}, {.demand = {2}, .tw = {0, 12}, .service = 5});
    b.add_client({0.0, 20.0}, {.demand = {3}, .tw = {50, 60}, .service = 5, .prize = 9});
    b.add_client({15.0, 15.0}, {.demand = {1}, .tw = {5, 25}, .service = 5});

    return b.build(0);
}

TEST_CASE("CostEvaluator: eval_splice_cost equals the change in route_cost",
          "[cost_evaluator][splice]") {
    auto data = make_tw_instance();
    std::vector<int> const base = {0, 1, 2, 3};
    std::vector<std::vector<int>> const seqs = {{}, {4}, {5}, {4, 5}, {5, 4}};

    CostEvaluator plain(7, 3);
    CostEvaluator piecewise(7, 3);
    piecewise.set_distance_cost_function(PiecewiseLinearFunction({{0, 0}, {50, 50}, {51, 53}}));
    piecewise.set_duration_cost_function(PiecewiseLinearFunction({{0, 0}, {60, 0}, {61, 2}}));

    Route route(data, 0);
    route.set_clients(base);
    int n = route.size();

    for (auto const* eval : {&plain, &piecewise}) {
        for (int first = 0; first <= n; ++first) {
            for (int last = first; last <= std::min(n, first + 2); ++last) {
                for (auto const& seq : seqs) {
                    std::vector<int> after(base.begin(), base.begin() + first);
                    after.insert(after.end(), seq.begin(), seq.end());
                    after.insert(after.end(), base.begin() + last, base.end());
                    Route spliced(data, 0);
                    spliced.set_clients(after);

                    INFO("first=" << first << " last=" << last << " |seq|=" << seq.size()
                                  << " piecewise=" << (eval == &piecewise));
                    CHECK(eval->eval_splice_cost(route, first, last, seq) ==
                          eval->route_cost(spliced) - eval->route_cost(route));
                }
            }
        }

        // Removing every client prices the route back to zero.
        Route whole(data, 0);
        whole.set_clients(base);
        CHECK(eval->eval_splice_cost(whole, 0, n, {}) == -eval->route_cost(whole));
    }
}

TEST_CASE("CostEvaluator: insert and remove deltas price time warp and duration cost",
          "[cost_evaluator][splice]") {
    auto data = make_tw_instance();
    CostEvaluator eval(7, 3);
    eval.set_duration_cost_function(PiecewiseLinearFunction({{0, 0}, {60, 0}, {61, 2}}));

    Route route(data, 0);
    route.set_clients({0, 1, 2});

    for (int pos = 0; pos <= route.size(); ++pos) {
        Route after = route;
        after.insert(pos, 4);
        CHECK(eval.eval_insert_cost(route, pos, 4) ==
              eval.route_cost(after) - eval.route_cost(route));
    }
    for (int pos = 0; pos < route.size(); ++pos) {
        Route after = route;
        after.remove(pos);
        CHECK(eval.eval_remove_cost(route, pos) == eval.route_cost(after) - eval.route_cost(route));
    }
}
