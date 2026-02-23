#include <catch2/catch_test_macros.hpp>

#include "routing/construction.h"
#include "routing/cost_evaluator.h"
#include "routing/local_search.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <algorithm>
#include <vector>

using namespace coso;

// ---------------------------------------------------------------------------
//  Test instance builders
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), 6 clients in a pattern that allows obvious improvements.
/// 2 vehicle types with capacity 15, 3 vehicles each.
static ProblemData make_small_instance(int granular_k = 0)
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {15}});

    b.add_client({10.0, 0.0}, {.demand = {3}});   // 0
    b.add_client({20.0, 0.0}, {.demand = {4}});   // 1
    b.add_client({30.0, 0.0}, {.demand = {5}});   // 2
    b.add_client({0.0, 10.0}, {.demand = {2}});   // 3
    b.add_client({0.0, 20.0}, {.demand = {3}});   // 4
    b.add_client({15.0, 15.0}, {.demand = {6}});  // 5

    return b.build(granular_k);
}

/// Larger instance: 1 depot, 12 clients arranged in two clusters.
/// Cluster A around (50,0), cluster B around (0,50).
static ProblemData make_clustered_instance(int granular_k = 0)
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(4, {.capacity = {30}});

    // Cluster A (near x=50)
    b.add_client({45.0, 0.0}, {.demand = {3}});   // 0
    b.add_client({50.0, 5.0}, {.demand = {4}});   // 1
    b.add_client({55.0, 0.0}, {.demand = {3}});   // 2
    b.add_client({50.0, -5.0}, {.demand = {4}});  // 3
    b.add_client({40.0, 5.0}, {.demand = {2}});   // 4
    b.add_client({60.0, -5.0}, {.demand = {3}});  // 5

    // Cluster B (near y=50)
    b.add_client({0.0, 45.0}, {.demand = {3}});   // 6
    b.add_client({5.0, 50.0}, {.demand = {4}});   // 7
    b.add_client({0.0, 55.0}, {.demand = {3}});   // 8
    b.add_client({-5.0, 50.0}, {.demand = {4}});  // 9
    b.add_client({5.0, 40.0}, {.demand = {2}});   // 10
    b.add_client({-5.0, 60.0}, {.demand = {3}});  // 11

    return b.build(granular_k);
}

/// Build a solution with given route assignments.
static Solution make_solution(ProblemData const& data,
                              std::vector<std::vector<int>> const& routes)
{
    Solution sol(data);
    for (int r = 0; r < static_cast<int>(routes.size()); ++r) {
        if (!routes[r].empty())
            sol.set_route_clients(r, routes[r]);
    }
    return sol;
}

/// Verify that every client appears exactly once across all routes.
static void check_all_clients_assigned(Solution const& sol,
                                       ProblemData const& data)
{
    CHECK(sol.num_unassigned() == 0);
    std::vector<int> count(data.num_clients(), 0);
    for (int r = 0; r < sol.num_routes(); ++r)
        for (int i = 0; i < sol.route(r).size(); ++i)
            count[sol.route(r).client(i)]++;
    for (int c = 0; c < data.num_clients(); ++c)
        CHECK(count[c] == 1);
}

// ===========================================================================
//  Basic local search tests
// ===========================================================================

TEST_CASE("LocalSearch: improves a badly ordered solution",
          "[localsearch]")
{
    auto data = make_small_instance();
    CostEvaluator eval(100);

    // All 6 clients in one route, badly ordered.
    auto sol = make_solution(data, {{2, 4, 0, 5, 1, 3}});
    int64_t old_cost = sol.cost(eval);

    LocalSearch ls(data);
    ls.run(sol, eval);

    int64_t new_cost = sol.cost(eval);
    CHECK(new_cost <= old_cost);
    CHECK(ls.last_num_moves() > 0);
    check_all_clients_assigned(sol, data);
}

TEST_CASE("LocalSearch: reaches a local optimum (no further improvement)",
          "[localsearch]")
{
    auto data = make_small_instance();
    CostEvaluator eval(100);

    // Start from a suboptimal solution.
    auto sol = make_solution(data, {{0, 5, 1}, {3, 2, 4}});

    LocalSearch ls(data);
    ls.run(sol, eval);

    int64_t cost_after_first = sol.cost(eval);

    // Running again should find no improvement (already at local optimum).
    ls.run(sol, eval);
    int64_t cost_after_second = sol.cost(eval);

    CHECK(cost_after_second == cost_after_first);
    CHECK(ls.last_num_moves() == 0);
    check_all_clients_assigned(sol, data);
}

TEST_CASE("LocalSearch: handles empty solution gracefully",
          "[localsearch]")
{
    auto data = make_small_instance();
    CostEvaluator eval(100);

    // No clients assigned at all (all unassigned).
    Solution sol(data);

    LocalSearch ls(data);
    ls.run(sol, eval);

    // Should not crash; no moves possible.
    CHECK(ls.last_num_moves() == 0);
}

TEST_CASE("LocalSearch: handles single-client routes",
          "[localsearch]")
{
    auto data = make_small_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0}, {1}, {2}});
    int64_t old_cost = sol.cost(eval);

    LocalSearch ls(data);
    ls.run(sol, eval);

    // Might or might not find improvements, but should not crash.
    CHECK(sol.cost(eval) <= old_cost);
}

// ===========================================================================
//  Clustered instance tests
// ===========================================================================

TEST_CASE("LocalSearch: separates inter-mixed clusters",
          "[localsearch][clustered]")
{
    auto data = make_clustered_instance();
    CostEvaluator eval(100);

    // Deliberately mix clients from clusters A and B across routes.
    auto sol = make_solution(data, {
        {0, 6, 2, 8},      // mixed A+B
        {1, 7, 3, 9},      // mixed A+B
        {4, 10, 5, 11},    // mixed A+B
    });
    int64_t old_cost = sol.cost(eval);

    LocalSearch ls(data);
    ls.run(sol, eval);

    int64_t new_cost = sol.cost(eval);
    CHECK(new_cost < old_cost);
    CHECK(ls.last_num_moves() > 0);
    check_all_clients_assigned(sol, data);
}

// ===========================================================================
//  Granular neighbourhood tests
// ===========================================================================

TEST_CASE("LocalSearch: works with granular neighbours",
          "[localsearch][granular]")
{
    auto data = make_small_instance(5);  // k=5 nearest neighbours
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{2, 4, 0, 5, 1, 3}});
    int64_t old_cost = sol.cost(eval);

    LocalSearch ls(data);
    ls.run(sol, eval);

    CHECK(sol.cost(eval) <= old_cost);
    check_all_clients_assigned(sol, data);
}

TEST_CASE("LocalSearch: granular clustered instance",
          "[localsearch][granular][clustered]")
{
    auto data = make_clustered_instance(8);  // k=8
    CostEvaluator eval(100);

    auto sol = make_solution(data, {
        {0, 6, 2, 8},
        {1, 7, 3, 9},
        {4, 10, 5, 11},
    });
    int64_t old_cost = sol.cost(eval);

    LocalSearch ls(data);
    ls.run(sol, eval);

    CHECK(sol.cost(eval) < old_cost);
    check_all_clients_assigned(sol, data);
}

// ===========================================================================
//  Construction + local search integration
// ===========================================================================

TEST_CASE("LocalSearch: improves nearest-neighbour solution",
          "[localsearch][construction]")
{
    auto data = make_clustered_instance();
    CostEvaluator eval(100);

    auto sol = construction::nearest_neighbour(data, eval);
    int64_t nn_cost = sol.cost(eval);

    LocalSearch ls(data);
    ls.run(sol, eval);

    int64_t ls_cost = sol.cost(eval);
    // Local search should not worsen the solution.
    CHECK(ls_cost <= nn_cost);
    check_all_clients_assigned(sol, data);
}

TEST_CASE("LocalSearch: improves Clarke-Wright solution",
          "[localsearch][construction]")
{
    auto data = make_clustered_instance();
    CostEvaluator eval(100);

    auto sol = construction::clarke_wright(data, eval);
    int64_t cw_cost = sol.cost(eval);

    LocalSearch ls(data);
    ls.run(sol, eval);

    int64_t ls_cost = sol.cost(eval);
    CHECK(ls_cost <= cw_cost);
    check_all_clients_assigned(sol, data);
}

// ===========================================================================
//  Capacity violation tests
// ===========================================================================

TEST_CASE("LocalSearch: reduces capacity violations",
          "[localsearch][capacity]")
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {10}});
    b.add_client({10.0, 0.0}, {.demand = {6}});   // 0
    b.add_client({20.0, 0.0}, {.demand = {6}});   // 1
    b.add_client({10.0, 10.0}, {.demand = {3}});  // 2
    b.add_client({20.0, 10.0}, {.demand = {3}});  // 3
    b.add_client({30.0, 0.0}, {.demand = {4}});   // 4
    b.add_client({30.0, 10.0}, {.demand = {2}});  // 5
    auto data = b.build(0);

    CostEvaluator eval(1000);  // high penalty

    // Route 0 = [0, 1, 4] demand=16 > cap=10 (overloaded).
    auto sol = make_solution(data, {{0, 1, 4}, {2, 3, 5}});
    int64_t old_cost = sol.cost(eval);

    LocalSearch ls(data);
    ls.run(sol, eval);

    int64_t new_cost = sol.cost(eval);
    CHECK(new_cost < old_cost);
    check_all_clients_assigned(sol, data);
}

// ===========================================================================
//  Statistics tests
// ===========================================================================

TEST_CASE("LocalSearch: reports iteration and move counts",
          "[localsearch][stats]")
{
    auto data = make_small_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{2, 4, 0, 5, 1, 3}});

    LocalSearch ls(data);
    ls.run(sol, eval);

    // Should have done at least one iteration.
    CHECK(ls.last_num_iters() >= 1);
    // Number of moves should be <= number of iterations.
    CHECK(ls.last_num_moves() <= ls.last_num_iters());
}
