#include <catch2/catch_test_macros.hpp>

#include "search/genetic_algorithm.h"
#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/stop_criterion.h"

using namespace coso;

// --------------------------------------------------------------------------- //
//  Helper: build a small CVRP instance for testing.                            //
// --------------------------------------------------------------------------- //

/// 1 depot at (0,0), 6 clients in a line, 2 vehicles with capacity 20.
static ProblemData make_ga_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {20}});

    b.add_client({10.0, 0.0}, {.demand = {3}});   // 0
    b.add_client({20.0, 0.0}, {.demand = {4}});   // 1
    b.add_client({30.0, 0.0}, {.demand = {5}});   // 2
    b.add_client({40.0, 0.0}, {.demand = {2}});   // 3
    b.add_client({50.0, 0.0}, {.demand = {1}});   // 4
    b.add_client({60.0, 0.0}, {.demand = {3}});   // 5

    return b.build(0);
}

/// Slightly larger instance: 10 clients in a grid, 3 vehicles.
static ProblemData make_larger_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {30}});

    b.add_client({10.0, 0.0},  {.demand = {3}});
    b.add_client({20.0, 0.0},  {.demand = {4}});
    b.add_client({30.0, 0.0},  {.demand = {5}});
    b.add_client({0.0,  10.0}, {.demand = {2}});
    b.add_client({10.0, 10.0}, {.demand = {6}});
    b.add_client({20.0, 10.0}, {.demand = {3}});
    b.add_client({30.0, 10.0}, {.demand = {4}});
    b.add_client({0.0,  20.0}, {.demand = {2}});
    b.add_client({10.0, 20.0}, {.demand = {1}});
    b.add_client({20.0, 20.0}, {.demand = {5}});

    return b.build(0);
}

// =========================================================================== //
//  Basic construction and configuration                                        //
// =========================================================================== //

TEST_CASE("GeneticAlgorithm: constructs without error",
          "[search][ga]")
{
    auto data = make_ga_instance();
    GeneticAlgorithm ga(data);

    // Should be configurable without crashing.
    ga.set_population_size(10, 10);
    ga.set_initial_population_size(8);
    ga.set_seed(123);
}

// =========================================================================== //
//  Run produces a valid solution                                               //
// =========================================================================== //

TEST_CASE("GeneticAlgorithm: run returns a solution with all clients assigned",
          "[search][ga]")
{
    auto data = make_ga_instance();
    CostEvaluator eval;
    StopCriterion stop(0.0, 50, 0);  // 50 iterations max

    GeneticAlgorithm ga(data);
    ga.set_population_size(5, 5);
    ga.set_initial_population_size(6);
    ga.set_seed(42);

    auto sol = ga.run(eval, stop);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
}

TEST_CASE("GeneticAlgorithm: run with time limit",
          "[search][ga]")
{
    auto data = make_ga_instance();
    CostEvaluator eval;
    StopCriterion stop(0.5);  // 0.5 seconds

    GeneticAlgorithm ga(data);
    ga.set_population_size(5, 5);
    ga.set_initial_population_size(6);
    ga.set_seed(99);

    auto sol = ga.run(eval, stop);

    CHECK(sol.num_unassigned() == 0);
    // Should terminate within reasonable time (the time limit).
    CHECK(stop.elapsed() < 2.0);
}

// =========================================================================== //
//  Solution quality improves over iterations                                   //
// =========================================================================== //

TEST_CASE("GeneticAlgorithm: more iterations yield equal or better solutions",
          "[search][ga]")
{
    auto data = make_larger_instance();
    CostEvaluator eval;

    // Short run.
    StopCriterion stop_short(0.0, 10, 0);
    GeneticAlgorithm ga_short(data);
    ga_short.set_population_size(5, 5);
    ga_short.set_initial_population_size(6);
    ga_short.set_seed(42);
    auto sol_short = ga_short.run(eval, stop_short);
    auto cost_short = sol_short.cost(eval);

    // Longer run.
    StopCriterion stop_long(0.0, 100, 0);
    GeneticAlgorithm ga_long(data);
    ga_long.set_population_size(5, 5);
    ga_long.set_initial_population_size(6);
    ga_long.set_seed(42);
    auto sol_long = ga_long.run(eval, stop_long);
    auto cost_long = sol_long.cost(eval);

    // Longer run should find at least as good a solution.
    CHECK(cost_long <= cost_short);
}

// =========================================================================== //
//  Feasibility                                                                 //
// =========================================================================== //

TEST_CASE("GeneticAlgorithm: returns feasible solution on easy instance",
          "[search][ga]")
{
    auto data = make_ga_instance();
    CostEvaluator eval;
    StopCriterion stop(0.0, 30, 0);

    GeneticAlgorithm ga(data);
    ga.set_population_size(5, 5);
    ga.set_initial_population_size(6);
    ga.set_seed(7);

    auto sol = ga.run(eval, stop);

    REQUIRE(sol.feasible());
    CHECK(sol.num_unassigned() == 0);

    // Verify no route exceeds capacity.
    for (int v = 0; v < sol.num_routes(); ++v) {
        CHECK(sol.route(v).load_excess() == 0);
    }
}

// =========================================================================== //
//  Reproducibility with seed                                                   //
// =========================================================================== //

TEST_CASE("GeneticAlgorithm: same seed produces same result",
          "[search][ga]")
{
    auto data = make_ga_instance();
    CostEvaluator eval;

    auto run_ga = [&](uint64_t seed) {
        StopCriterion stop(0.0, 30, 0);
        GeneticAlgorithm ga(data);
        ga.set_population_size(5, 5);
        ga.set_initial_population_size(6);
        ga.set_seed(seed);
        return ga.run(eval, stop);
    };

    auto sol1 = run_ga(42);
    auto sol2 = run_ga(42);

    CHECK(sol1.cost(eval) == sol2.cost(eval));
}

TEST_CASE("GeneticAlgorithm: different seeds can produce different results",
          "[search][ga]")
{
    auto data = make_larger_instance();
    CostEvaluator eval;

    auto run_ga = [&](uint64_t seed) {
        StopCriterion stop(0.0, 50, 0);
        GeneticAlgorithm ga(data);
        ga.set_population_size(5, 5);
        ga.set_initial_population_size(6);
        ga.set_seed(seed);
        return ga.run(eval, stop);
    };

    auto sol1 = run_ga(1);
    auto sol2 = run_ga(999);

    // Different seeds may produce different costs (not guaranteed, but likely
    // on a non-trivial instance with enough iterations).
    // We just check both are valid.
    CHECK(sol1.num_unassigned() == 0);
    CHECK(sol2.num_unassigned() == 0);
}

// =========================================================================== //
//  Population size configuration                                               //
// =========================================================================== //

TEST_CASE("GeneticAlgorithm: custom population sizes work",
          "[search][ga]")
{
    auto data = make_ga_instance();
    CostEvaluator eval;
    StopCriterion stop(0.0, 20, 0);

    GeneticAlgorithm ga(data);
    ga.set_population_size(3, 3);
    ga.set_initial_population_size(4);
    ga.set_seed(42);

    auto sol = ga.run(eval, stop);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
}
