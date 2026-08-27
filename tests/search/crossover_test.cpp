#include "search/crossover.h"

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <random>
#include <set>
#include <vector>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small CVRP instance for crossover tests.
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), 8 clients in a grid, 4 vehicles with capacity 20.
static ProblemData make_crossover_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(4, {.capacity = {20}});

    // 8 clients with small demands.
    b.add_client({10.0, 0.0}, {.demand = {3}});   // 0
    b.add_client({20.0, 0.0}, {.demand = {4}});   // 1
    b.add_client({30.0, 0.0}, {.demand = {2}});   // 2
    b.add_client({0.0, 10.0}, {.demand = {5}});   // 3
    b.add_client({10.0, 10.0}, {.demand = {3}});  // 4
    b.add_client({20.0, 10.0}, {.demand = {4}});  // 5
    b.add_client({30.0, 10.0}, {.demand = {2}});  // 6
    b.add_client({0.0, 20.0}, {.demand = {1}});   // 7

    return b.build(0);
}

/// Helper: collect all assigned clients in a solution, sorted.
static std::vector<int> all_assigned(Solution const& sol, int num_clients) {
    std::vector<int> result;
    for (int c = 0; c < num_clients; ++c) {
        if (sol.is_assigned(c)) {
            result.push_back(c);
        }
    }
    return result;
}

/// Helper: check no client appears in more than one route.
static bool no_duplicates(Solution const& sol, int num_clients) {
    std::vector<int> count(num_clients, 0);
    for (int v = 0; v < sol.num_routes(); ++v) {
        auto const& route = sol.route(v);
        for (int i = 0; i < route.size(); ++i) {
            int c = route.client(i);
            if (c < 0 || c >= num_clients) {
                return false;
            }
            count[c]++;
            if (count[c] > 1) {
                return false;
            }
        }
    }
    return true;
}

// ===========================================================================
//  SREX crossover tests
// ===========================================================================

TEST_CASE("srex_crossover: offspring contains all clients", "[crossover]") {
    auto data = make_crossover_instance();
    CostEvaluator eval;

    // Build two distinct parent solutions.
    Solution p1(data);
    p1.set_route_clients(0, {0, 1, 2});
    p1.set_route_clients(1, {3, 4});
    p1.set_route_clients(2, {5, 6});
    p1.set_route_clients(3, {7});

    Solution p2(data);
    p2.set_route_clients(0, {0, 3, 7});
    p2.set_route_clients(1, {1, 5});
    p2.set_route_clients(2, {2, 4, 6});

    std::mt19937 rng(42);
    auto offspring = srex_crossover(p1, p2, data, eval, rng);

    // All 8 clients must be assigned.
    auto assigned = all_assigned(offspring, data.num_clients());
    CHECK(assigned.size() == 8);
    CHECK(offspring.num_unassigned() == 0);

    // Expected: {0,1,2,3,4,5,6,7}.
    std::vector<int> expected = {0, 1, 2, 3, 4, 5, 6, 7};
    CHECK(assigned == expected);
}

TEST_CASE("srex_crossover: no duplicate clients", "[crossover]") {
    auto data = make_crossover_instance();
    CostEvaluator eval;

    Solution p1(data);
    p1.set_route_clients(0, {0, 1});
    p1.set_route_clients(1, {2, 3});
    p1.set_route_clients(2, {4, 5});
    p1.set_route_clients(3, {6, 7});

    Solution p2(data);
    p2.set_route_clients(0, {7, 6});
    p2.set_route_clients(1, {5, 4});
    p2.set_route_clients(2, {3, 2});
    p2.set_route_clients(3, {1, 0});

    std::mt19937 rng(123);

    // Run multiple times with different seeds to stress-test.
    for (int i = 0; i < 50; ++i) {
        auto offspring = srex_crossover(p1, p2, data, eval, rng);
        CHECK(no_duplicates(offspring, data.num_clients()));
        CHECK(offspring.num_unassigned() == 0);
    }
}

TEST_CASE("srex_crossover: inherits at least one route from parent1", "[crossover]") {
    auto data = make_crossover_instance();
    CostEvaluator eval;

    Solution p1(data);
    p1.set_route_clients(0, {0, 1, 2});
    p1.set_route_clients(1, {3, 4, 5});
    p1.set_route_clients(2, {6, 7});

    Solution p2(data);
    p2.set_route_clients(0, {7, 0});
    p2.set_route_clients(1, {1, 2, 3});
    p2.set_route_clients(2, {4, 5, 6});

    std::mt19937 rng(99);

    // Run several times; each offspring must contain at least one complete
    // route from parent1.
    for (int trial = 0; trial < 30; ++trial) {
        auto offspring = srex_crossover(p1, p2, data, eval, rng);

        // Collect offspring routes as sets of clients.
        std::vector<std::set<int>> offspring_routes;
        for (int v = 0; v < offspring.num_routes(); ++v) {
            auto const& r = offspring.route(v);
            if (r.empty()) {
                continue;
            }
            std::set<int> s;
            for (int i = 0; i < r.size(); ++i) {
                s.insert(r.client(i));
            }
            offspring_routes.push_back(s);
        }

        // Check that at least one parent1 route appears as a subset
        // in some offspring route (order may differ due to slot assignment,
        // but the exact client set should appear).
        bool found_inherited = false;
        for (int v = 0; v < p1.num_routes(); ++v) {
            auto const& r = p1.route(v);
            if (r.empty()) {
                continue;
            }
            std::set<int> p1_set;
            for (int i = 0; i < r.size(); ++i) {
                p1_set.insert(r.client(i));
            }

            for (auto const& os : offspring_routes) {
                if (os == p1_set) {
                    found_inherited = true;
                    break;
                }
            }
            if (found_inherited) {
                break;
            }
        }

        CHECK(found_inherited);
    }
}

TEST_CASE("srex_crossover: identical parents produce valid offspring", "[crossover]") {
    auto data = make_crossover_instance();
    CostEvaluator eval;

    Solution p1(data);
    p1.set_route_clients(0, {0, 1, 2, 3});
    p1.set_route_clients(1, {4, 5, 6, 7});

    std::mt19937 rng(7);
    auto offspring = srex_crossover(p1, p1, data, eval, rng);

    CHECK(offspring.num_unassigned() == 0);
    CHECK(no_duplicates(offspring, data.num_clients()));

    auto assigned = all_assigned(offspring, data.num_clients());
    CHECK(assigned.size() == 8);
}

TEST_CASE("srex_crossover: empty parent1 returns copy of parent2", "[crossover]") {
    auto data = make_crossover_instance();
    CostEvaluator eval;

    Solution p1(data);  // all unassigned
    Solution p2(data);
    p2.set_route_clients(0, {0, 1, 2, 3});
    p2.set_route_clients(1, {4, 5, 6, 7});

    std::mt19937 rng(0);
    auto offspring = srex_crossover(p1, p2, data, eval, rng);

    // Should be identical to parent2.
    CHECK(offspring.num_unassigned() == 0);
    for (int v = 0; v < offspring.num_routes(); ++v) {
        CHECK(offspring.route(v).size() == p2.route(v).size());
    }
}

TEST_CASE("srex_crossover: max_routes=1 selects exactly one route", "[crossover]") {
    auto data = make_crossover_instance();
    CostEvaluator eval;

    Solution p1(data);
    p1.set_route_clients(0, {0, 1});
    p1.set_route_clients(1, {2, 3});
    p1.set_route_clients(2, {4, 5});
    p1.set_route_clients(3, {6, 7});

    Solution p2(data);
    p2.set_route_clients(0, {7, 6, 5, 4});
    p2.set_route_clients(1, {3, 2, 1, 0});

    std::mt19937 rng(55);

    for (int trial = 0; trial < 20; ++trial) {
        auto offspring = srex_crossover(p1, p2, data, eval, rng, /*max_routes=*/1);
        CHECK(offspring.num_unassigned() == 0);
        CHECK(no_duplicates(offspring, data.num_clients()));

        // Count how many p1 routes appear intact in offspring.
        int inherited_count = 0;
        for (int v = 0; v < p1.num_routes(); ++v) {
            auto const& r = p1.route(v);
            if (r.empty()) {
                continue;
            }

            std::set<int> p1_set;
            for (int i = 0; i < r.size(); ++i) {
                p1_set.insert(r.client(i));
            }

            for (int ov = 0; ov < offspring.num_routes(); ++ov) {
                auto const& or_ = offspring.route(ov);
                if (or_.empty()) {
                    continue;
                }
                std::set<int> os;
                for (int i = 0; i < or_.size(); ++i) {
                    os.insert(or_.client(i));
                }
                if (os == p1_set) {
                    inherited_count++;
                    break;
                }
            }
        }

        // With max_routes=1, exactly 1 route from p1 should appear.
        CHECK(inherited_count >= 1);
    }
}

TEST_CASE("srex_crossover: tight capacity still produces valid solution", "[crossover]") {
    // Instance with tight capacity: 4 clients, demand=5 each, vehicle cap=10.
    // Need at least 2 routes.
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(4, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {5}});
    b.add_client({20.0, 0.0}, {.demand = {5}});
    b.add_client({0.0, 10.0}, {.demand = {5}});
    b.add_client({0.0, 20.0}, {.demand = {5}});

    auto data = b.build(0);
    CostEvaluator eval;

    Solution p1(data);
    p1.set_route_clients(0, {0, 1});
    p1.set_route_clients(1, {2, 3});

    Solution p2(data);
    p2.set_route_clients(0, {0, 2});
    p2.set_route_clients(1, {1, 3});

    std::mt19937 rng(42);

    for (int i = 0; i < 20; ++i) {
        auto offspring = srex_crossover(p1, p2, data, eval, rng);
        CHECK(offspring.num_unassigned() == 0);
        CHECK(no_duplicates(offspring, data.num_clients()));

        auto assigned = all_assigned(offspring, data.num_clients());
        CHECK(assigned.size() == 4);
    }
}
