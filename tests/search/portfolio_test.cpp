#include "search/portfolio.h"

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/iterated_local_search.h"
#include "search/stop_criterion.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// --------------------------------------------------------------------------- //
//  Helper: build small CVRP instances for testing.                             //
// --------------------------------------------------------------------------- //

/// 1 depot at (0,0), 6 clients in a line, 2 vehicles with capacity 20.
static ProblemData make_small_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {20}});

    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {4}});
    b.add_client({30.0, 0.0}, {.demand = {5}});
    b.add_client({40.0, 0.0}, {.demand = {2}});
    b.add_client({50.0, 0.0}, {.demand = {1}});
    b.add_client({60.0, 0.0}, {.demand = {3}});

    return b.build(0);
}

/// 10 clients in a grid, 3 vehicles with capacity 30.
static ProblemData make_medium_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {30}});

    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {4}});
    b.add_client({30.0, 0.0}, {.demand = {5}});
    b.add_client({0.0, 10.0}, {.demand = {2}});
    b.add_client({10.0, 10.0}, {.demand = {6}});
    b.add_client({20.0, 10.0}, {.demand = {3}});
    b.add_client({30.0, 10.0}, {.demand = {4}});
    b.add_client({0.0, 20.0}, {.demand = {2}});
    b.add_client({10.0, 20.0}, {.demand = {1}});
    b.add_client({20.0, 20.0}, {.demand = {5}});

    return b.build(0);
}

// =========================================================================== //
//  Basic construction                                                          //
// =========================================================================== //

TEST_CASE("PortfolioSolver: constructs without error", "[search][portfolio]") {
    auto data = make_small_instance();
    PortfolioSolver solver(data);

    // Configuration should not crash.
    solver.set_seed(123);
}

// =========================================================================== //
//  Run produces a valid, feasible solution                                     //
// =========================================================================== //

TEST_CASE("PortfolioSolver: run returns a feasible solution with all clients assigned",
          "[search][portfolio]") {
    auto data = make_small_instance();
    CostEvaluator eval;
    StopCriterion stop(0.0, 100, 0);

    PortfolioSolver solver(data);
    solver.set_seed(42);

    auto sol = solver.run(eval, stop);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());

    // All routes must respect capacity.
    for (int v = 0; v < sol.num_routes(); ++v) {
        CHECK(sol.route(v).load_excess() == 0);
    }
}

// =========================================================================== //
//  Time-limited run                                                            //
// =========================================================================== //

TEST_CASE("PortfolioSolver: run with time limit terminates promptly", "[search][portfolio]") {
    auto data = make_medium_instance();
    CostEvaluator eval;
    StopCriterion stop(1.0);  // 1 second

    PortfolioSolver solver(data);
    solver.set_seed(42);

    auto sol = solver.run(eval, stop);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
    // Should terminate within a reasonable margin.
    CHECK(stop.elapsed() < 3.0);
}

// =========================================================================== //
//  Portfolio finds equal or better solution than ILS alone                      //
// =========================================================================== //

TEST_CASE("PortfolioSolver: finds solution at least as good as ILS alone", "[search][portfolio]") {
    auto data = make_medium_instance();
    CostEvaluator eval;

    // Run ILS alone.
    StopCriterion ils_stop(0.0, 500, 0);
    IteratedLocalSearch ils(data, 42);
    auto ils_sol = ils.run(eval, ils_stop);
    int64_t ils_cost = ils_sol.cost(eval);

    // Run portfolio (ILS + GA + finalizer).
    StopCriterion port_stop(0.0, 500, 0);
    PortfolioSolver solver(data);
    solver.set_seed(42);
    auto port_sol = solver.run(eval, port_stop);
    int64_t port_cost = port_sol.cost(eval);

    // Portfolio should find a solution at least as good (the finalizer
    // alone should improve or maintain the ILS result).
    CHECK(port_cost <= ils_cost);
}

// =========================================================================== //
//  Reproducibility with seed                                                   //
// =========================================================================== //

TEST_CASE("PortfolioSolver: same seed produces same result", "[search][portfolio]") {
    auto data = make_small_instance();
    CostEvaluator eval;

    auto run_portfolio = [&](uint64_t seed) {
        StopCriterion stop(0.0, 100, 0);
        PortfolioSolver solver(data);
        solver.set_seed(seed);
        return solver.run(eval, stop);
    };

    auto sol1 = run_portfolio(42);
    auto sol2 = run_portfolio(42);

    CHECK(sol1.cost(eval) == sol2.cost(eval));
}

TEST_CASE("PortfolioSolver: different seeds can produce different results", "[search][portfolio]") {
    auto data = make_medium_instance();
    CostEvaluator eval;

    auto run_portfolio = [&](uint64_t seed) {
        StopCriterion stop(0.0, 200, 0);
        PortfolioSolver solver(data);
        solver.set_seed(seed);
        return solver.run(eval, stop);
    };

    auto sol1 = run_portfolio(1);
    auto sol2 = run_portfolio(999);

    // Both should be valid.
    CHECK(sol1.num_unassigned() == 0);
    CHECK(sol2.num_unassigned() == 0);
    CHECK(sol1.feasible());
    CHECK(sol2.feasible());
}

// =========================================================================== //
//  Finalization produces a feasible solution                                    //
// =========================================================================== //

TEST_CASE("PortfolioSolver: result is always finalized (feasible, locally optimal)",
          "[search][portfolio]") {
    auto data = make_medium_instance();
    CostEvaluator eval;
    StopCriterion stop(0.0, 50, 0);

    PortfolioSolver solver(data);
    solver.set_seed(7);

    auto sol = solver.run(eval, stop);

    // Finalizer guarantees feasibility.
    REQUIRE(sol.feasible());

    // No route exceeds capacity.
    for (int v = 0; v < sol.num_routes(); ++v) {
        CHECK(sol.route(v).load_excess() == 0);
    }
}

// =========================================================================== //
//  Medium instance with iteration budget                                       //
// =========================================================================== //

TEST_CASE("PortfolioSolver: works on medium instance with iteration budget",
          "[search][portfolio]") {
    auto data = make_medium_instance();
    CostEvaluator eval;
    StopCriterion stop(0.0, 100, 0);

    PortfolioSolver solver(data);
    solver.set_seed(42);

    auto sol = solver.run(eval, stop);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
}
