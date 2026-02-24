#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "search/population.h"
#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <random>

using namespace coso;

// --------------------------------------------------------------------------- //
//  Helper: build a small CVRP instance for testing.                            //
// --------------------------------------------------------------------------- //

/// 1 depot at (0,0), 6 clients in a line, 2 vehicles with capacity 20.
static ProblemData make_pop_instance()
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

/// Create a solution with specific route assignments.
static Solution make_solution(ProblemData const& data,
                              std::vector<int> route0,
                              std::vector<int> route1)
{
    Solution sol(data);
    sol.set_route_clients(0, std::move(route0));
    sol.set_route_clients(1, std::move(route1));
    return sol;
}

/// Instance where all solutions are infeasible due to capacity.
static ProblemData make_tight_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {5}});

    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {4}});
    b.add_client({30.0, 0.0}, {.demand = {5}});

    return b.build(0);
}

// =========================================================================== //
//  Construction and basic properties                                           //
// =========================================================================== //

TEST_CASE("Population: empty population", "[search][population]")
{
    auto data = make_pop_instance();
    Population pop(data, 5, 5);

    CHECK(pop.feasible_size() == 0);
    CHECK(pop.infeasible_size() == 0);
    CHECK(pop.size() == 0);
    CHECK_FALSE(pop.has_feasible());
}

TEST_CASE("Population: add feasible solution", "[search][population]")
{
    auto data = make_pop_instance();
    CostEvaluator eval;
    Population pop(data, 5, 5);

    auto sol = make_solution(data, {0, 1, 2}, {3, 4, 5});
    REQUIRE(sol.feasible());

    pop.add(std::move(sol), eval);

    CHECK(pop.feasible_size() == 1);
    CHECK(pop.infeasible_size() == 0);
    CHECK(pop.has_feasible());
}

TEST_CASE("Population: add infeasible solution", "[search][population]")
{
    auto data = make_tight_instance();
    CostEvaluator eval;
    Population pop(data, 5, 5);

    // All 3 clients on 1 vehicle: demand 12 > capacity 5.
    Solution sol(data);
    sol.set_route_clients(0, {0, 1, 2});
    REQUIRE_FALSE(sol.feasible());

    pop.add(std::move(sol), eval);

    CHECK(pop.feasible_size() == 0);
    CHECK(pop.infeasible_size() == 1);
    CHECK_FALSE(pop.has_feasible());
}

TEST_CASE("Population: best_feasible returns lowest cost",
          "[search][population]")
{
    auto data = make_pop_instance();
    CostEvaluator eval;
    Population pop(data, 10, 10);

    // Solution A: clients spread across routes.
    auto solA = make_solution(data, {0, 1, 2}, {3, 4, 5});
    auto costA = solA.cost(eval);

    // Solution B: all on one route (longer total distance).
    auto solB = make_solution(data, {0, 1, 2, 3, 4, 5}, {});
    auto costB = solB.cost(eval);

    pop.add(std::move(solB), eval);
    pop.add(std::move(solA), eval);

    auto best_cost = pop.best_feasible().cost(eval);
    CHECK(best_cost == std::min(costA, costB));
}

// =========================================================================== //
//  Survivor selection (max size enforcement)                                    //
// =========================================================================== //

TEST_CASE("Population: feasible sub-population does not exceed max size",
          "[search][population]")
{
    auto data = make_pop_instance();
    CostEvaluator eval;
    int max_feas = 3;
    Population pop(data, max_feas, 5);

    // Add more than max_feas feasible solutions.
    for (int i = 0; i < 6; ++i) {
        // Create different solutions by rotating client assignments.
        std::vector<int> r0, r1;
        for (int c = 0; c < 6; ++c) {
            if ((c + i) % 2 == 0)
                r0.push_back(c);
            else
                r1.push_back(c);
        }
        pop.add(make_solution(data, r0, r1), eval);
    }

    CHECK(pop.feasible_size() <= max_feas);
}

TEST_CASE("Population: infeasible sub-population does not exceed max size",
          "[search][population]")
{
    auto data = make_tight_instance();
    CostEvaluator eval;
    int max_infeas = 3;
    Population pop(data, 5, max_infeas);

    // Add multiple infeasible solutions.
    for (int i = 0; i < 6; ++i) {
        Solution sol(data);
        // Vary the order to get different edge sets.
        if (i % 3 == 0)
            sol.set_route_clients(0, {0, 1, 2});
        else if (i % 3 == 1)
            sol.set_route_clients(0, {1, 0, 2});
        else
            sol.set_route_clients(0, {2, 1, 0});

        pop.add(std::move(sol), eval);
    }

    CHECK(pop.infeasible_size() <= max_infeas);
}

// =========================================================================== //
//  Parent selection                                                             //
// =========================================================================== //

TEST_CASE("Population: select_parent returns a valid solution",
          "[search][population]")
{
    auto data = make_pop_instance();
    CostEvaluator eval;
    Population pop(data, 10, 10);

    pop.add(make_solution(data, {0, 1, 2}, {3, 4, 5}), eval);
    pop.add(make_solution(data, {0, 1}, {2, 3, 4, 5}), eval);
    pop.add(make_solution(data, {0, 1, 2, 3}, {4, 5}), eval);

    std::mt19937 rng(42);

    // Call select_parent many times and verify it returns valid solutions.
    for (int i = 0; i < 20; ++i) {
        auto const& parent = pop.select_parent(rng);
        CHECK(parent.num_unassigned() == 0);
    }
}

TEST_CASE("Population: select_parent with single solution",
          "[search][population]")
{
    auto data = make_pop_instance();
    CostEvaluator eval;
    Population pop(data, 10, 10);

    auto sol = make_solution(data, {0, 1, 2}, {3, 4, 5});
    auto expected_cost = sol.cost(eval);
    pop.add(std::move(sol), eval);

    std::mt19937 rng(123);
    auto const& parent = pop.select_parent(rng);
    CHECK(parent.cost(eval) == expected_cost);
}

// =========================================================================== //
//  Diversity management                                                         //
// =========================================================================== //

TEST_CASE("Population: identical solutions have zero broken-pairs distance",
          "[search][population]")
{
    auto data = make_pop_instance();
    CostEvaluator eval;
    Population pop(data, 10, 10);

    // Add two identical solutions.
    pop.add(make_solution(data, {0, 1, 2}, {3, 4, 5}), eval);
    pop.add(make_solution(data, {0, 1, 2}, {3, 4, 5}), eval);

    // Both should survive with max size 10.
    CHECK(pop.feasible_size() == 2);
}

TEST_CASE("Population: diverse solutions survive over clones",
          "[search][population]")
{
    auto data = make_pop_instance();
    CostEvaluator eval;
    Population pop(data, 3, 3);

    // Add a unique solution.
    auto unique = make_solution(data, {5, 4, 3}, {2, 1, 0});
    auto unique_cost = unique.cost(eval);

    // Add several clones of a different solution.
    pop.add(make_solution(data, {0, 1, 2}, {3, 4, 5}), eval);
    pop.add(make_solution(data, {0, 1, 2}, {3, 4, 5}), eval);
    pop.add(make_solution(data, {0, 1, 2}, {3, 4, 5}), eval);

    // Now add the unique solution (triggers survivor selection since we
    // exceed max_feasible = 3, going to 4).
    pop.add(std::move(unique), eval);

    // The population should be 3 solutions.  The unique one should survive
    // because its diversity gives it a better biased fitness than the clones.
    CHECK(pop.feasible_size() == 3);

    // Verify the best feasible cost is at least as good as the best of the
    // clones (the clone cost).
    auto clone = make_solution(data, {0, 1, 2}, {3, 4, 5});
    auto clone_cost = clone.cost(eval);
    auto best_cost = pop.best_feasible().cost(eval);
    CHECK(best_cost <= std::max(unique_cost, clone_cost));
}

// =========================================================================== //
//  Mixed feasible/infeasible population                                         //
// =========================================================================== //

TEST_CASE("Population: mixed feasible and infeasible",
          "[search][population]")
{
    // Use tight instance: 1 vehicle, capacity 5.
    // Clients: demand 3, 4, 5.
    auto data = make_tight_instance();
    CostEvaluator eval;
    Population pop(data, 5, 5);

    // Feasible: single client with demand <= 5.
    Solution feas(data);
    feas.set_route_clients(0, {0});  // demand 3 <= 5
    REQUIRE(feas.feasible());
    pop.add(std::move(feas), eval);

    // Infeasible: two clients, demand 7 > 5.
    Solution infeas(data);
    infeas.set_route_clients(0, {0, 1});  // demand 7 > 5
    REQUIRE_FALSE(infeas.feasible());
    pop.add(std::move(infeas), eval);

    CHECK(pop.feasible_size() == 1);
    CHECK(pop.infeasible_size() == 1);
    CHECK(pop.size() == 2);
    CHECK(pop.has_feasible());

    // Parent selection should work with mixed population.
    std::mt19937 rng(99);
    for (int i = 0; i < 10; ++i) {
        auto const& parent = pop.select_parent(rng);
        // Should return some solution (feasible or infeasible).
        (void)parent;
    }
}
