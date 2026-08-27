#include "routing/piecewise_cost.h"

#include "routing/cost_evaluator.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ===========================================================================
//  PiecewiseLinearFunction: construction
// ===========================================================================

TEST_CASE("PiecewiseLinearFunction: requires at least 2 breakpoints", "[piecewise_cost]") {
    CHECK_THROWS_AS(PiecewiseLinearFunction({{0, 0}}), std::invalid_argument);

    CHECK_THROWS_AS(PiecewiseLinearFunction({}), std::invalid_argument);
}

TEST_CASE("PiecewiseLinearFunction: requires strictly increasing x", "[piecewise_cost]") {
    CHECK_THROWS_AS(PiecewiseLinearFunction({{0, 0}, {0, 1}}), std::invalid_argument);

    CHECK_THROWS_AS(PiecewiseLinearFunction({{0, 0}, {5, 5}, {3, 3}}), std::invalid_argument);
}

TEST_CASE("PiecewiseLinearFunction: valid construction", "[piecewise_cost]") {
    PiecewiseLinearFunction f({{0, 0}, {10, 20}});
    CHECK(f.num_breakpoints() == 2);
}

// ===========================================================================
//  PiecewiseLinearFunction: evaluation
// ===========================================================================

TEST_CASE("PiecewiseLinearFunction: simple linear (identity-like)", "[piecewise_cost]") {
    // y = 2x, defined by (0,0) and (10,20).
    PiecewiseLinearFunction f({{0, 0}, {10, 20}});

    CHECK(f.evaluate(0) == 0);
    CHECK(f.evaluate(5) == 10);
    CHECK(f.evaluate(10) == 20);
}

TEST_CASE("PiecewiseLinearFunction: extrapolation below first breakpoint", "[piecewise_cost]") {
    // Slope 2, starting at (10, 20).
    PiecewiseLinearFunction f({{10, 20}, {20, 40}});

    // At x=0: y = 20 + (0-10)*2 = 0.
    CHECK(f.evaluate(0) == 0);
    // At x=5: y = 20 + (5-10)*2 = 10.
    CHECK(f.evaluate(5) == 10);
}

TEST_CASE("PiecewiseLinearFunction: extrapolation above last breakpoint", "[piecewise_cost]") {
    PiecewiseLinearFunction f({{0, 0}, {10, 20}});

    // Slope 2. At x=15: y = 20 + (15-10)*2 = 30 (extrapolating from segment [0,10]).
    // Actually extrapolation uses the last segment's slope.
    // Last segment is [0,10] with slope 2.
    // At x=15: y = 0 + 15*2 = 30. Same since there's only one segment.
    CHECK(f.evaluate(15) == 30);
    CHECK(f.evaluate(20) == 40);
}

TEST_CASE("PiecewiseLinearFunction: multi-segment interpolation", "[piecewise_cost]") {
    // Three segments:
    //   [0,0] -> [100, 100]   slope = 1
    //   [100,100] -> [200, 300]  slope = 2
    //   [200,300] -> [300, 600]  slope = 3
    PiecewiseLinearFunction f({{0, 0}, {100, 100}, {200, 300}, {300, 600}});

    // First segment (slope 1).
    CHECK(f.evaluate(0) == 0);
    CHECK(f.evaluate(50) == 50);
    CHECK(f.evaluate(100) == 100);

    // Second segment (slope 2).
    CHECK(f.evaluate(150) == 200);
    CHECK(f.evaluate(200) == 300);

    // Third segment (slope 3).
    CHECK(f.evaluate(250) == 450);
    CHECK(f.evaluate(300) == 600);

    // Extrapolation beyond last breakpoint (slope 3).
    CHECK(f.evaluate(350) == 750);
}

TEST_CASE("PiecewiseLinearFunction: negative slope segments", "[piecewise_cost]") {
    // Discount after threshold: cost goes down.
    PiecewiseLinearFunction f({{0, 0}, {100, 200}, {200, 150}});

    CHECK(f.evaluate(0) == 0);
    CHECK(f.evaluate(100) == 200);
    CHECK(f.evaluate(150) == 175);  // midpoint, slope = -0.5 per unit
    CHECK(f.evaluate(200) == 150);
}

// ===========================================================================
//  PiecewiseLinearFunction: delta computation
// ===========================================================================

TEST_CASE("PiecewiseLinearFunction: delta same x returns zero", "[piecewise_cost]") {
    PiecewiseLinearFunction f({{0, 0}, {100, 200}});
    CHECK(f.delta(50, 50) == 0);
}

TEST_CASE("PiecewiseLinearFunction: delta on same segment is O(1)", "[piecewise_cost]") {
    PiecewiseLinearFunction f({{0, 0}, {100, 200}, {200, 500}});

    // Both on first segment (slope 2).
    CHECK(f.delta(20, 50) == 60);   // (50-20)*2 = 60
    CHECK(f.delta(50, 20) == -60);  // reverse

    // Both on second segment (slope 3).
    CHECK(f.delta(120, 180) == 180);  // (180-120)*3 = 180
}

TEST_CASE("PiecewiseLinearFunction: delta across segments equals evaluate diff",
          "[piecewise_cost]") {
    PiecewiseLinearFunction f({{0, 0}, {100, 100}, {200, 300}});

    int64_t d = f.delta(50, 150);
    CHECK(d == f.evaluate(150) - f.evaluate(50));
}

// ===========================================================================
//  Factory: tiered pricing
// ===========================================================================

TEST_CASE("PiecewiseLinearFunction::tiered: basic two-tier", "[piecewise_cost]") {
    // First 100 units at rate 1, beyond at rate 3.
    auto f = PiecewiseLinearFunction::tiered({100}, {1, 3});

    CHECK(f.evaluate(0) == 0);
    CHECK(f.evaluate(50) == 50);
    CHECK(f.evaluate(100) == 100);
    CHECK(f.evaluate(150) == 250);  // 100 + 50*3
    CHECK(f.evaluate(200) == 400);  // 100 + 100*3
}

TEST_CASE("PiecewiseLinearFunction::tiered: three-tier", "[piecewise_cost]") {
    // 0-100 at rate 1, 100-300 at rate 2, 300+ at rate 5.
    auto f = PiecewiseLinearFunction::tiered({100, 300}, {1, 2, 5});

    CHECK(f.evaluate(0) == 0);
    CHECK(f.evaluate(100) == 100);
    CHECK(f.evaluate(200) == 300);   // 100 + 100*2
    CHECK(f.evaluate(300) == 500);   // 100 + 200*2
    CHECK(f.evaluate(400) == 1000);  // 500 + 100*5
}

TEST_CASE("PiecewiseLinearFunction::tiered: requires rates.size() == thresholds.size() + 1",
          "[piecewise_cost]") {
    CHECK_THROWS_AS(PiecewiseLinearFunction::tiered({100}, {1}), std::invalid_argument);

    CHECK_THROWS_AS(PiecewiseLinearFunction::tiered({100}, {1, 2, 3}), std::invalid_argument);
}

// ===========================================================================
//  Factory: overtime
// ===========================================================================

TEST_CASE("PiecewiseLinearFunction::overtime: single tier", "[piecewise_cost]") {
    // Free up to 480 (8 hours), then 10 per unit.
    auto f = PiecewiseLinearFunction::overtime(480, 10);

    CHECK(f.evaluate(0) == 0);
    CHECK(f.evaluate(240) == 0);
    CHECK(f.evaluate(480) == 0);
    CHECK(f.evaluate(490) == 100);  // 10 units OT * 10
    CHECK(f.evaluate(500) == 200);
}

TEST_CASE("PiecewiseLinearFunction::overtime: two tiers", "[piecewise_cost]") {
    // Free up to 480, then 10/unit for 120 units, then 20/unit.
    auto f = PiecewiseLinearFunction::overtime(480, 10, 120, 20);

    CHECK(f.evaluate(0) == 0);
    CHECK(f.evaluate(480) == 0);
    CHECK(f.evaluate(540) == 600);   // 60 * 10
    CHECK(f.evaluate(600) == 1200);  // 120 * 10
    CHECK(f.evaluate(650) == 2200);  // 1200 + 50*20
}

// ===========================================================================
//  Integration with CostEvaluator
// ===========================================================================

/// 1 depot at (0,0), 3 clients on x-axis, capacity 100.
static ProblemData make_piecewise_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {100}, .cost = {.fixed_cost = 0, .unit_distance_cost = 1}});

    b.add_client({10.0, 0.0}, {.demand = {1}});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {1}});  // client 1
    b.add_client({50.0, 0.0}, {.demand = {1}});  // client 2

    return b.build(0);
}

TEST_CASE("CostEvaluator: piecewise distance cost replaces linear",
          "[piecewise_cost][cost_evaluator]") {
    auto data = make_piecewise_instance();
    Route route(data, 0);
    route.set_clients({0, 1});
    // Distance: depot(0)->c0(10)->c1(20)->depot(0) = 10+10+20 = 40.

    // Linear: 40 * 1 = 40.
    CostEvaluator eval_linear;
    CHECK(eval_linear.route_objective(route) == 40);

    // Piecewise: first 20 units at rate 1, beyond at rate 3.
    // f(40) = 20 + 20*3 = 80.
    CostEvaluator eval_pw;
    eval_pw.set_distance_cost_function(PiecewiseLinearFunction::tiered({20}, {1, 3}));

    CHECK(eval_pw.route_objective(route) == 80);
    CHECK(eval_pw.has_distance_cost_function());
}

TEST_CASE("CostEvaluator: clear piecewise reverts to linear", "[piecewise_cost][cost_evaluator]") {
    auto data = make_piecewise_instance();
    Route route(data, 0);
    route.set_clients({0, 1});

    CostEvaluator eval;
    eval.set_distance_cost_function(PiecewiseLinearFunction::tiered({20}, {1, 3}));
    CHECK(eval.route_objective(route) == 80);

    eval.clear_distance_cost_function();
    CHECK_FALSE(eval.has_distance_cost_function());
    CHECK(eval.route_objective(route) == 40);
}

TEST_CASE("CostEvaluator: piecewise delta evaluation for insert",
          "[piecewise_cost][cost_evaluator]") {
    auto data = make_piecewise_instance();
    CostEvaluator eval;
    eval.set_distance_cost_function(PiecewiseLinearFunction::tiered({20}, {1, 3}));

    Route route(data, 0);
    route.set_clients({0, 1});
    int64_t old_cost = eval.route_cost(route);

    // Test inserting client 2 at every position.
    for (int pos = 0; pos <= route.size(); ++pos) {
        int64_t delta = eval.eval_insert_cost(route, pos, 2);

        Route copy(data, 0);
        copy.set_clients({0, 1});
        copy.insert(pos, 2);
        int64_t new_cost = eval.route_cost(copy);

        CHECK(delta == new_cost - old_cost);
    }
}

TEST_CASE("CostEvaluator: piecewise delta evaluation for remove",
          "[piecewise_cost][cost_evaluator]") {
    auto data = make_piecewise_instance();
    CostEvaluator eval;
    eval.set_distance_cost_function(PiecewiseLinearFunction::tiered({20}, {1, 3}));

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

TEST_CASE("CostEvaluator: tiered pricing scenario end-to-end", "[piecewise_cost][cost_evaluator]") {
    // Scenario: delivery company charges per-km rates:
    //   0-100 km: $1/km
    //   100-300 km: $2/km
    //   300+ km: $5/km
    auto f = PiecewiseLinearFunction::tiered({100, 300}, {1, 2, 5});

    auto data = make_piecewise_instance();
    CostEvaluator eval;
    eval.set_distance_cost_function(std::move(f));

    // Route with distance 40 (0-100 tier): cost = 40*1 = 40.
    Route short_route(data, 0);
    short_route.set_clients({0, 1});
    CHECK(eval.route_objective(short_route) == 40);

    // Route with distance 100 (depot->c2->depot = 100): cost = 100*1 = 100.
    Route long_route(data, 0);
    long_route.set_clients({2});
    CHECK(long_route.distance() == 100);
    CHECK(eval.route_objective(long_route) == 100);
}
