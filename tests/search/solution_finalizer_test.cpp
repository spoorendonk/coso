#include "search/solution_finalizer.h"

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/stop_criterion.h"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace coso;

// ---------------------------------------------------------------------------
//  Test instance builders
// ---------------------------------------------------------------------------

/// Build a solution with given route assignments.
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

/// 1 depot at origin, 6 clients, 3 vehicles with capacity 10.
static ProblemData make_small_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {3}});   // 0
    b.add_client({20.0, 0.0}, {.demand = {4}});   // 1
    b.add_client({30.0, 0.0}, {.demand = {5}});   // 2
    b.add_client({0.0, 10.0}, {.demand = {2}});   // 3
    b.add_client({0.0, 20.0}, {.demand = {3}});   // 4
    b.add_client({15.0, 15.0}, {.demand = {6}});  // 5

    return b.build(0);
}

/// Instance where one route is overloaded: cap=10, route has demand 16.
static ProblemData make_overloaded_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {6}});   // 0
    b.add_client({20.0, 0.0}, {.demand = {6}});   // 1
    b.add_client({10.0, 10.0}, {.demand = {3}});  // 2
    b.add_client({20.0, 10.0}, {.demand = {3}});  // 3
    b.add_client({30.0, 0.0}, {.demand = {4}});   // 4
    b.add_client({30.0, 10.0}, {.demand = {2}});  // 5

    return b.build(0);
}

// ===========================================================================
//  Basic finalization tests
// ===========================================================================

TEST_CASE("SolutionFinalizer: feasible solution stays feasible", "[finalizer]") {
    auto data = make_small_instance();
    // All demands fit within capacity 10.
    auto sol = make_solution(data, {{0, 1}, {2, 3}, {4, 5}});
    // d0=3+4=7, d1=5+2=7, d2=3+6=9  -- all <= 10.
    REQUIRE(sol.feasible());
    int old_dist = sol.total_distance();

    SolutionFinalizer fin(data);
    fin.finalize(sol);

    CHECK(sol.feasible());
    CHECK(sol.total_distance() <= old_dist);
}

TEST_CASE("SolutionFinalizer: improves distance on feasible solution", "[finalizer]") {
    auto data = make_small_instance();
    // Badly ordered but feasible: route 0 has clients from different
    // directions, route 1 has mixed clients.
    auto sol = make_solution(data, {{2, 3}, {0, 4}, {1, 5}});
    REQUIRE(sol.feasible());
    int old_dist = sol.total_distance();

    SolutionFinalizer fin(data);
    fin.finalize(sol);

    CHECK(sol.feasible());
    // Finalizer should find inter-route moves to improve distance.
    CHECK(sol.total_distance() <= old_dist);
}

TEST_CASE("SolutionFinalizer: repairs infeasible solution", "[finalizer]") {
    auto data = make_overloaded_instance();

    // Route 0 = [0, 1, 4] -> demand = 6+6+4 = 16 > cap 10.
    auto sol = make_solution(data, {{0, 1, 4}, {2, 3, 5}});
    REQUIRE(!sol.feasible());

    SolutionFinalizer fin(data);
    fin.finalize(sol);

    // After finalization, the solution must be feasible.
    CHECK(sol.feasible());
}

TEST_CASE("SolutionFinalizer: empty solution stays empty", "[finalizer]") {
    auto data = make_small_instance();
    Solution sol(data);

    SolutionFinalizer fin(data);
    fin.finalize(sol);

    CHECK(sol.feasible());
    CHECK(sol.num_unassigned() == data.num_clients());
}

TEST_CASE("SolutionFinalizer: single-client routes remain feasible", "[finalizer]") {
    auto data = make_small_instance();
    auto sol = make_solution(data, {{0}, {1}, {2}});
    REQUIRE(sol.feasible());

    SolutionFinalizer fin(data);
    fin.finalize(sol);

    CHECK(sol.feasible());
}

TEST_CASE("SolutionFinalizer: result is locally optimal under high penalty", "[finalizer]") {
    auto data = make_small_instance();
    auto sol = make_solution(data, {{2, 3}, {0, 4}, {1, 5}});

    SolutionFinalizer fin(data);
    fin.finalize(sol);

    // Running finalize again should not change anything — already at local
    // optimum.
    int dist_before = sol.total_distance();
    fin.finalize(sol);
    CHECK(sol.total_distance() == dist_before);
    CHECK(sol.feasible());
}

// ===========================================================================
//  Stop criterion tests
// ===========================================================================

TEST_CASE("SolutionFinalizer: respects stop criterion", "[finalizer][stop]") {
    auto data = make_small_instance();
    auto sol = make_solution(data, {{2, 3}, {0, 4}, {1, 5}});
    int dist_before = sol.total_distance();

    // Create an already-expired stop criterion (max 1 iteration, already used).
    StopCriterion stop(0.0, 1, 0);
    stop.iteration();  // iter_ == 1 >= max_iter_ == 1 → should_stop() == true

    SolutionFinalizer fin(data);
    fin.finalize(sol, &stop);

    // With an expired stop, the finalizer should not modify the solution
    // (local search exits immediately).
    CHECK(sol.total_distance() == dist_before);
}

TEST_CASE("SolutionFinalizer: nullptr stop runs to completion", "[finalizer][stop]") {
    auto data = make_small_instance();
    auto sol = make_solution(data, {{2, 3}, {0, 4}, {1, 5}});

    SolutionFinalizer fin(data);
    fin.finalize(sol, nullptr);

    CHECK(sol.feasible());
}
