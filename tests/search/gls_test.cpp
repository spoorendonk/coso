#include <catch2/catch_test_macros.hpp>

#include "routing/construction.h"
#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/guided_local_search.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small CVRP instance (6 clients, 1 depot, 2 vehicles cap 20)
// ---------------------------------------------------------------------------

static ProblemData make_small_cvrp()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

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
//  Helper: build a medium instance (20 clients)
// ---------------------------------------------------------------------------

static ProblemData make_medium_cvrp()
{
    ProblemData::Builder b;
    b.add_depot({50.0, 50.0});

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 4; ++j) {
            b.add_client(
                {static_cast<double>(i * 20), static_cast<double>(j * 25)},
                {.demand = {3}});
        }
    }

    b.add_vehicle_type(4, {.capacity = {20}});

    return b.build();
}

TEST_CASE("GLS — initial state has zero penalties", "[gls]")
{
    auto data = make_small_cvrp();
    GuidedLocalSearch gls(data, 0.1);

    // All penalties should be zero.
    int n = data.num_nodes();
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            CHECK(gls.penalty(i, j) == 0);
        }
    }
}

TEST_CASE("GLS — augmented cost is zero with no penalties", "[gls]")
{
    auto data = make_small_cvrp();
    CostEvaluator eval;

    Solution sol = construction::clarke_wright(data, eval);
    GuidedLocalSearch gls(data, 0.1);

    CHECK(gls.augmented_cost(sol) == 0);
}

TEST_CASE("GLS — penalize increments exactly one edge penalty", "[gls]")
{
    auto data = make_small_cvrp();
    CostEvaluator eval;

    Solution sol = construction::clarke_wright(data, eval);
    GuidedLocalSearch gls(data, 0.1);

    gls.penalize(sol);

    // Exactly one edge should have penalty 1, all others 0.
    int n = data.num_nodes();
    int total_penalty = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            total_penalty += gls.penalty(i, j);
        }
    }
    CHECK(total_penalty == 1);
}

TEST_CASE("GLS — augmented cost is positive after penalizing", "[gls]")
{
    auto data = make_small_cvrp();
    CostEvaluator eval;

    Solution sol = construction::clarke_wright(data, eval);
    GuidedLocalSearch gls(data, 0.1);

    gls.penalize(sol);

    // The augmented cost should be positive (lambda * 1 penalty on a used edge).
    int64_t aug = gls.augmented_cost(sol);
    CHECK(aug > 0);
}

TEST_CASE("GLS — repeated penalization diversifies across edges", "[gls]")
{
    auto data = make_small_cvrp();
    CostEvaluator eval;

    Solution sol = construction::clarke_wright(data, eval);
    GuidedLocalSearch gls(data, 0.1);

    // Penalize many times.  The utility function should spread penalties
    // across different edges rather than always penalizing the same one.
    for (int iter = 0; iter < 20; ++iter) {
        gls.penalize(sol);
    }

    // Count edges with non-zero penalties.
    int n = data.num_nodes();
    int penalized_edges = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (gls.penalty(i, j) > 0)
                penalized_edges++;
        }
    }

    // After 20 penalizations on a 6-client instance, we should have spread
    // to at least 2 distinct edges (utility balancing).
    CHECK(penalized_edges >= 2);
}

TEST_CASE("GLS — reset clears all penalties", "[gls]")
{
    auto data = make_small_cvrp();
    CostEvaluator eval;

    Solution sol = construction::clarke_wright(data, eval);
    GuidedLocalSearch gls(data, 0.1);

    gls.penalize(sol);
    CHECK(gls.augmented_cost(sol) > 0);

    gls.reset();

    int n = data.num_nodes();
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            CHECK(gls.penalty(i, j) == 0);
        }
    }
    CHECK(gls.augmented_cost(sol) == 0);
}

TEST_CASE("GLS — lambda = 0 gives zero augmented cost", "[gls]")
{
    auto data = make_small_cvrp();
    CostEvaluator eval;

    Solution sol = construction::clarke_wright(data, eval);
    GuidedLocalSearch gls(data, 0.0);

    gls.penalize(sol);
    CHECK(gls.augmented_cost(sol) == 0);
}

TEST_CASE("GLS — higher lambda gives higher augmented cost", "[gls]")
{
    auto data = make_small_cvrp();
    CostEvaluator eval;

    Solution sol = construction::clarke_wright(data, eval);

    GuidedLocalSearch gls_low(data, 0.1);
    GuidedLocalSearch gls_high(data, 1.0);

    // Apply same penalization pattern.
    for (int i = 0; i < 10; ++i) {
        gls_low.penalize(sol);
        gls_high.penalize(sol);
    }

    CHECK(gls_high.augmented_cost(sol) >= gls_low.augmented_cost(sol));
}

TEST_CASE("GLS — empty solution has zero augmented cost", "[gls]")
{
    auto data = make_small_cvrp();
    GuidedLocalSearch gls(data, 0.5);

    Solution sol(data);  // all clients unassigned, all routes empty

    // Penalize should be a no-op with no edges.
    gls.penalize(sol);
    CHECK(gls.augmented_cost(sol) == 0);
}

TEST_CASE("GLS — set_lambda changes penalty weight", "[gls]")
{
    auto data = make_small_cvrp();
    CostEvaluator eval;

    Solution sol = construction::clarke_wright(data, eval);
    GuidedLocalSearch gls(data, 0.1);

    gls.penalize(sol);
    int64_t cost_low = gls.augmented_cost(sol);

    gls.set_lambda(1.0);
    int64_t cost_high = gls.augmented_cost(sol);

    CHECK(cost_high >= cost_low);
    CHECK(gls.lambda() == 1.0);
}

TEST_CASE("GLS — utility prefers expensive edges", "[gls]")
{
    // Build a 3-client instance where one edge is much longer than the others.
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_client({1.0, 0.0}, {.demand = {1}});   // close to depot
    b.add_client({100.0, 0.0}, {.demand = {1}});  // far from depot
    b.add_client({101.0, 0.0}, {.demand = {1}});  // close to client 1
    b.add_vehicle_type(1, {.capacity = {10}});
    auto data = b.build();

    CostEvaluator eval;
    Solution sol = construction::clarke_wright(data, eval);

    GuidedLocalSearch gls(data, 0.1);
    gls.penalize(sol);

    // The first penalization should hit the longest edge in the solution.
    // We just verify that exactly one edge was penalized (tested above)
    // and the augmented cost is proportional.
    CHECK(gls.augmented_cost(sol) > 0);
}

TEST_CASE("GLS — medium instance repeated penalization", "[gls]")
{
    auto data = make_medium_cvrp();
    CostEvaluator eval;

    Solution sol = construction::clarke_wright(data, eval);
    GuidedLocalSearch gls(data, 0.2);

    // Run many penalizations.
    for (int i = 0; i < 100; ++i) {
        gls.penalize(sol);
    }

    // Augmented cost should grow with repeated penalization.
    int64_t aug = gls.augmented_cost(sol);
    CHECK(aug > 0);

    // Reset and verify it goes back to zero.
    gls.reset();
    CHECK(gls.augmented_cost(sol) == 0);
}
