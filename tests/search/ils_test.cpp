#include <catch2/catch_test_macros.hpp>

#include "routing/construction.h"
#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/iterated_local_search.h"
#include "search/stop_criterion.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small CVRP instance (6 clients, 1 depot, 2 vehicles cap 20)
// ---------------------------------------------------------------------------

static ProblemData make_small_cvrp()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

    // Clients arranged around the depot.
    b.add_client({10.0, 0.0},  {.demand = {5}});
    b.add_client({10.0, 10.0}, {.demand = {5}});
    b.add_client({0.0, 10.0},  {.demand = {5}});
    b.add_client({-10.0, 0.0}, {.demand = {5}});
    b.add_client({-10.0, -10.0}, {.demand = {5}});
    b.add_client({0.0, -10.0}, {.demand = {5}});

    b.add_vehicle_type(2, {.capacity = {20}});

    return b.build();
}

// ---------------------------------------------------------------------------
//  Helper: build a larger instance (20 clients)
// ---------------------------------------------------------------------------

static ProblemData make_medium_cvrp()
{
    ProblemData::Builder b;
    b.add_depot({50.0, 50.0});

    // 20 clients in a grid pattern.
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 4; ++j) {
            b.add_client(
                {static_cast<double>(i * 20), static_cast<double>(j * 25)},
                {.demand = {3}});
        }
    }

    // 4 vehicles with capacity 20.
    b.add_vehicle_type(4, {.capacity = {20}});

    return b.build();
}

TEST_CASE("ILS — basic run with iteration limit", "[ils]")
{
    auto data = make_small_cvrp();
    CostEvaluator eval;

    IteratedLocalSearch ils(data);
    StopCriterion stop(0.0, 50, 0);  // 50 iterations

    Solution best = ils.run(eval, stop);

    // All clients should be assigned.
    CHECK(best.num_unassigned() == 0);

    // Cost should be finite and positive.
    int64_t cost = best.cost(eval);
    CHECK(cost > 0);
}

TEST_CASE("ILS — improves over construction heuristic", "[ils]")
{
    auto data = make_medium_cvrp();
    CostEvaluator eval;

    // Get baseline cost from construction heuristic.
    Solution cw = construction::clarke_wright(data, eval);
    int64_t cw_cost = cw.cost(eval);

    // Run ILS.
    IteratedLocalSearch ils(data, 123);
    StopCriterion stop(0.0, 200, 50);

    Solution best = ils.run(eval, stop);
    int64_t ils_cost = best.cost(eval);

    // ILS should produce a solution no worse than construction alone.
    CHECK(ils_cost <= cw_cost);
    CHECK(best.num_unassigned() == 0);
}

TEST_CASE("ILS — time limit stops search", "[ils]")
{
    auto data = make_small_cvrp();
    CostEvaluator eval;

    IteratedLocalSearch ils(data);
    StopCriterion stop(0.1);  // 100ms time limit

    Solution best = ils.run(eval, stop);

    CHECK(best.num_unassigned() == 0);
    CHECK(stop.elapsed() >= 0.1);
    CHECK(stop.iterations() > 0);
}

TEST_CASE("ILS — no-improve limit stops search", "[ils]")
{
    auto data = make_small_cvrp();
    CostEvaluator eval;

    IteratedLocalSearch ils(data);
    StopCriterion stop(10.0, 0, 20);  // long time limit, no-improve = 20

    Solution best = ils.run(eval, stop);

    CHECK(best.num_unassigned() == 0);
    CHECK(stop.iterations_no_improve() >= 20);
}

TEST_CASE("ILS — different seeds produce different trajectories", "[ils]")
{
    auto data = make_medium_cvrp();
    CostEvaluator eval;

    // Two runs with different seeds.
    StopCriterion stop1(0.0, 100, 0);
    IteratedLocalSearch ils1(data, 1);
    Solution sol1 = ils1.run(eval, stop1);

    StopCriterion stop2(0.0, 100, 0);
    IteratedLocalSearch ils2(data, 999);
    Solution sol2 = ils2.run(eval, stop2);

    // Both should be valid solutions.
    CHECK(sol1.num_unassigned() == 0);
    CHECK(sol2.num_unassigned() == 0);

    // They might have different costs (not guaranteed, but likely).
    // At minimum, both should have positive cost.
    CHECK(sol1.cost(eval) > 0);
    CHECK(sol2.cost(eval) > 0);
}

TEST_CASE("ILS — single client instance", "[ils]")
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_client({10.0, 0.0}, {.demand = {5}});
    b.add_vehicle_type(1, {.capacity = {10}});
    auto data = b.build();

    CostEvaluator eval;
    IteratedLocalSearch ils(data);
    StopCriterion stop(0.0, 10, 0);

    Solution best = ils.run(eval, stop);

    CHECK(best.num_unassigned() == 0);
    CHECK(best.num_used_vehicles() == 1);
}

TEST_CASE("ILS — custom ruin fraction", "[ils]")
{
    auto data = make_medium_cvrp();
    CostEvaluator eval;

    IteratedLocalSearch ils(data, 42);
    ils.set_ruin_fraction_min(0.2);
    ils.set_ruin_fraction_max(0.5);
    ils.set_late_acceptance_length(100);

    StopCriterion stop(0.0, 50, 0);
    Solution best = ils.run(eval, stop);

    CHECK(best.num_unassigned() == 0);
    CHECK(best.cost(eval) > 0);
}
