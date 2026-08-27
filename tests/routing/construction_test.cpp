#include "routing/construction.h"

#include "routing/cost_evaluator.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build small CVRP instances for testing.
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), 5 clients in a line, 3 vehicles with capacity 10.
/// Client demands: 3, 4, 5, 2, 6 => total 20.
/// Vehicle capacity 10 each => need at least 2 vehicles.
static ProblemData make_line_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {3}});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {4}});  // client 1
    b.add_client({30.0, 0.0}, {.demand = {5}});  // client 2
    b.add_client({40.0, 0.0}, {.demand = {2}});  // client 3
    b.add_client({50.0, 0.0}, {.demand = {6}});  // client 4

    return b.build(0);
}

/// 1 depot at origin, 6 clients in a cluster arrangement, 4 vehicles cap 8.
/// Two clusters: (10,10), (12,10), (10,12) and (−10,−10), (−12,−10), (−10,−12).
/// Demands: 2, 3, 2, 2, 3, 2 => total 14.
static ProblemData make_cluster_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(4, {.capacity = {8}});

    // Cluster A
    b.add_client({10.0, 10.0}, {.demand = {2}});  // 0
    b.add_client({12.0, 10.0}, {.demand = {3}});  // 1
    b.add_client({10.0, 12.0}, {.demand = {2}});  // 2

    // Cluster B
    b.add_client({-10.0, -10.0}, {.demand = {2}});  // 3
    b.add_client({-12.0, -10.0}, {.demand = {3}});  // 4
    b.add_client({-10.0, -12.0}, {.demand = {2}});  // 5

    return b.build(0);
}

/// Tight capacity: 4 clients with demand 5 each, 2 vehicles with capacity 10.
/// Exactly 2 routes needed, 2 clients per route.
static ProblemData make_tight_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {5}});  // 0
    b.add_client({20.0, 0.0}, {.demand = {5}});  // 1
    b.add_client({0.0, 10.0}, {.demand = {5}});  // 2
    b.add_client({0.0, 20.0}, {.demand = {5}});  // 3

    return b.build(0);
}

/// Heterogeneous fleet: 2 small vehicles (cap 8) + 1 large vehicle (cap 20).
/// 5 clients with demands: 3, 4, 5, 2, 6. Total 20.
/// Large vehicle can take up to 20, small ones 8 each.
static ProblemData make_hetero_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {8}});
    b.add_vehicle_type(1, {.capacity = {20}});

    b.add_client({10.0, 0.0}, {.demand = {3}});  // 0
    b.add_client({20.0, 0.0}, {.demand = {4}});  // 1
    b.add_client({30.0, 0.0}, {.demand = {5}});  // 2
    b.add_client({0.0, 10.0}, {.demand = {2}});  // 3
    b.add_client({0.0, 20.0}, {.demand = {6}});  // 4

    return b.build(0);
}

/// Single client instance (edge case).
static ProblemData make_single_client_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {10}});
    b.add_client({10.0, 0.0}, {.demand = {3}});

    return b.build(0);
}

/// Empty instance (no clients).
static ProblemData make_empty_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {10}});

    return b.build(0);
}

// ===========================================================================
//  Nearest-neighbour tests
// ===========================================================================

TEST_CASE("NN: all clients assigned in line instance", "[construction][nn]") {
    auto data = make_line_instance();
    CostEvaluator eval;
    auto sol = construction::nearest_neighbour(data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
}

TEST_CASE("NN: capacity not violated", "[construction][nn]") {
    auto data = make_line_instance();
    CostEvaluator eval;
    auto sol = construction::nearest_neighbour(data, eval);

    for (int v = 0; v < sol.num_routes(); ++v) {
        CHECK(sol.route(v).load_feasible());
    }
}

TEST_CASE("NN: cluster instance groups nearby clients", "[construction][nn]") {
    auto data = make_cluster_instance();
    CostEvaluator eval;
    auto sol = construction::nearest_neighbour(data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
    // With capacity 8 and demands 2+3+2=7 per cluster, NN should use ~2 routes.
    CHECK(sol.num_used_vehicles() >= 2);
}

TEST_CASE("NN: tight capacity uses correct number of vehicles", "[construction][nn]") {
    auto data = make_tight_instance();
    CostEvaluator eval;
    auto sol = construction::nearest_neighbour(data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
    CHECK(sol.num_used_vehicles() == 2);
}

TEST_CASE("NN: heterogeneous fleet", "[construction][nn]") {
    auto data = make_hetero_instance();
    CostEvaluator eval;
    auto sol = construction::nearest_neighbour(data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
}

TEST_CASE("NN: single client", "[construction][nn]") {
    auto data = make_single_client_instance();
    CostEvaluator eval;
    auto sol = construction::nearest_neighbour(data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
    CHECK(sol.num_used_vehicles() == 1);
}

TEST_CASE("NN: empty instance", "[construction][nn]") {
    auto data = make_empty_instance();
    CostEvaluator eval;
    auto sol = construction::nearest_neighbour(data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.num_used_vehicles() == 0);
}

// ===========================================================================
//  Clarke-Wright savings tests
// ===========================================================================

TEST_CASE("CW: all clients assigned in line instance", "[construction][cw]") {
    auto data = make_line_instance();
    CostEvaluator eval;
    auto sol = construction::clarke_wright(data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
}

TEST_CASE("CW: capacity not violated", "[construction][cw]") {
    auto data = make_line_instance();
    CostEvaluator eval;
    auto sol = construction::clarke_wright(data, eval);

    for (int v = 0; v < sol.num_routes(); ++v) {
        CHECK(sol.route(v).load_feasible());
    }
}

TEST_CASE("CW: cluster instance groups nearby clients", "[construction][cw]") {
    auto data = make_cluster_instance();
    CostEvaluator eval;
    auto sol = construction::clarke_wright(data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
    // Savings heuristic should efficiently group clusters.
    CHECK(sol.num_used_vehicles() >= 2);
}

TEST_CASE("CW: tight capacity uses correct number of vehicles", "[construction][cw]") {
    auto data = make_tight_instance();
    CostEvaluator eval;
    auto sol = construction::clarke_wright(data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
    CHECK(sol.num_used_vehicles() == 2);
}

TEST_CASE("CW: heterogeneous fleet", "[construction][cw]") {
    auto data = make_hetero_instance();
    CostEvaluator eval;
    auto sol = construction::clarke_wright(data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
}

TEST_CASE("CW: single client", "[construction][cw]") {
    auto data = make_single_client_instance();
    CostEvaluator eval;
    auto sol = construction::clarke_wright(data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.feasible());
    CHECK(sol.num_used_vehicles() == 1);
}

TEST_CASE("CW: empty instance", "[construction][cw]") {
    auto data = make_empty_instance();
    CostEvaluator eval;
    auto sol = construction::clarke_wright(data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.num_used_vehicles() == 0);
}

// ===========================================================================
//  Comparison: Clarke-Wright should beat nearest-neighbour on distance
// ===========================================================================

TEST_CASE("CW produces shorter total distance than NN on cluster instance",
          "[construction][comparison]") {
    auto data = make_cluster_instance();
    CostEvaluator eval;

    auto nn_sol = construction::nearest_neighbour(data, eval);
    auto cw_sol = construction::clarke_wright(data, eval);

    // Both should be feasible with all clients assigned.
    REQUIRE(nn_sol.num_unassigned() == 0);
    REQUIRE(cw_sol.num_unassigned() == 0);
    REQUIRE(nn_sol.feasible());
    REQUIRE(cw_sol.feasible());

    // Clarke-Wright should produce equal or better total distance.
    CHECK(cw_sol.total_distance() <= nn_sol.total_distance());
}

TEST_CASE("CW produces shorter or equal cost than NN on line instance",
          "[construction][comparison]") {
    auto data = make_line_instance();
    CostEvaluator eval;

    auto nn_sol = construction::nearest_neighbour(data, eval);
    auto cw_sol = construction::clarke_wright(data, eval);

    REQUIRE(nn_sol.num_unassigned() == 0);
    REQUIRE(cw_sol.num_unassigned() == 0);
    REQUIRE(nn_sol.feasible());
    REQUIRE(cw_sol.feasible());

    CHECK(cw_sol.cost(eval) <= nn_sol.cost(eval));
}
