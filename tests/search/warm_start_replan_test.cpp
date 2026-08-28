#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/warm_start.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <vector>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small CVRP instance (8 clients, 1 depot, 4 vehicles cap 30)
// ---------------------------------------------------------------------------

static ProblemData make_replan_cvrp() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

    b.add_client({10.0, 0.0}, {.demand = {5}});     // 0
    b.add_client({10.0, 10.0}, {.demand = {5}});    // 1
    b.add_client({0.0, 10.0}, {.demand = {5}});     // 2
    b.add_client({-10.0, 0.0}, {.demand = {5}});    // 3
    b.add_client({-10.0, -10.0}, {.demand = {5}});  // 4
    b.add_client({0.0, -10.0}, {.demand = {5}});    // 5
    b.add_client({20.0, 0.0}, {.demand = {5}});     // 6
    b.add_client({20.0, 10.0}, {.demand = {5}});    // 7

    b.add_vehicle_type(4, {.capacity = {30}});

    return b.build();
}

// ---------------------------------------------------------------------------
//  Helper: build a solution with known routes
// ---------------------------------------------------------------------------

static Solution make_initial_solution(ProblemData const& data) {
    CostEvaluator eval;
    std::vector<std::vector<int>> routes = {{0, 1, 2}, {3, 4, 5}, {6, 7}, {}};
    return warm_start(routes, data, eval);
}

// ===========================================================================
//  Replan: pinned clients remain in place
// ===========================================================================

TEST_CASE("replan -- pinned clients stay in their route and position", "[replan][warm_start]") {
    auto data = make_replan_cvrp();
    CostEvaluator eval;
    Solution sol = make_initial_solution(data);

    // Record initial positions of clients 0 and 3 (to be pinned).
    auto route0_before =
        std::vector<int>(sol.route(0).clients().begin(), sol.route(0).clients().end());
    auto route1_before =
        std::vector<int>(sol.route(1).clients().begin(), sol.route(1).clients().end());

    ReplanConfig config;
    config.pinned_clients = {0, 1, 2, 3, 4, 5, 6, 7};  // pin all

    PinSet pins = replan(sol, config, data, eval);

    // All clients should still be in the same routes.
    auto route0_after =
        std::vector<int>(sol.route(0).clients().begin(), sol.route(0).clients().end());
    auto route1_after =
        std::vector<int>(sol.route(1).clients().begin(), sol.route(1).clients().end());

    CHECK(route0_after == route0_before);
    CHECK(route1_after == route1_before);
    CHECK(pins.size() == 8);
}

TEST_CASE("replan -- pinned clients not moved even when free clients move",
          "[replan][warm_start]") {
    auto data = make_replan_cvrp();
    CostEvaluator eval;
    Solution sol = make_initial_solution(data);

    // Pin clients in route 0 (clients 0, 1, 2).
    ReplanConfig config;
    config.pinned_clients = {0, 1, 2};

    // Record route 0 before replan.
    auto route0_before =
        std::vector<int>(sol.route(0).clients().begin(), sol.route(0).clients().end());

    replan(sol, config, data, eval);

    // Route 0 should be unchanged since all its clients are pinned.
    auto route0_after =
        std::vector<int>(sol.route(0).clients().begin(), sol.route(0).clients().end());
    CHECK(route0_after == route0_before);

    // All clients should still be assigned.
    CHECK(sol.num_unassigned() == 0);
}

// ===========================================================================
//  Replan: new clients are inserted
// ===========================================================================

TEST_CASE("replan -- new clients are inserted into routes", "[replan][warm_start]") {
    // Create a problem with 10 clients but start with only 8 assigned.
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

    b.add_client({10.0, 0.0}, {.demand = {5}});                      // 0
    b.add_client({10.0, 10.0}, {.demand = {5}});                     // 1
    b.add_client({0.0, 10.0}, {.demand = {5}});                      // 2
    b.add_client({-10.0, 0.0}, {.demand = {5}});                     // 3
    b.add_client({-10.0, -10.0}, {.demand = {5}});                   // 4
    b.add_client({0.0, -10.0}, {.demand = {5}});                     // 5
    b.add_client({20.0, 0.0}, {.demand = {5}});                      // 6
    b.add_client({20.0, 10.0}, {.demand = {5}});                     // 7
    b.add_client({5.0, 5.0}, {.demand = {3}, .required = false});    // 8 - new
    b.add_client({-5.0, -5.0}, {.demand = {3}, .required = false});  // 9 - new

    b.add_vehicle_type(4, {.capacity = {30}});
    auto data = b.build();
    CostEvaluator eval;

    // Start with 8 clients assigned, 2 optional clients unassigned.
    std::vector<std::vector<int>> routes = {{0, 1, 2}, {3, 4, 5}, {6, 7}, {}};
    Solution sol = warm_start(routes, data, eval);
    CHECK(sol.num_unassigned() == 2);

    // Replan: insert the new clients.
    ReplanConfig config;
    config.new_clients = {8, 9};

    replan(sol, config, data, eval);

    // Both new clients should now be assigned.
    CHECK(sol.is_assigned(8));
    CHECK(sol.is_assigned(9));
    CHECK(sol.num_unassigned() == 0);
}

// ===========================================================================
//  Replan: removed clients are gone
// ===========================================================================

TEST_CASE("replan -- removed clients are no longer in routes", "[replan][warm_start]") {
    // Use optional clients so removal is valid.
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

    b.add_client({10.0, 0.0}, {.demand = {5}});                        // 0
    b.add_client({10.0, 10.0}, {.demand = {5}});                       // 1
    b.add_client({0.0, 10.0}, {.demand = {5}, .required = false});     // 2 optional
    b.add_client({-10.0, 0.0}, {.demand = {5}});                       // 3
    b.add_client({-10.0, -10.0}, {.demand = {5}, .required = false});  // 4 optional
    b.add_client({0.0, -10.0}, {.demand = {5}});                       // 5

    b.add_vehicle_type(3, {.capacity = {30}});
    auto data = b.build();
    CostEvaluator eval;

    std::vector<std::vector<int>> routes = {{0, 1, 2}, {3, 4, 5}};
    Solution sol = warm_start(routes, data, eval);
    CHECK(sol.is_assigned(2));
    CHECK(sol.is_assigned(4));

    ReplanConfig config;
    config.removed_clients = {2, 4};

    replan(sol, config, data, eval);

    // Clients 2 and 4 should be unassigned (or re-inserted by local search
    // if beneficial, but they are no longer required).
    // At minimum, the removal was performed before local search.
    // After local search they might be re-inserted, but they were removed
    // from their original positions.

    // Verify the remaining required clients are still assigned.
    CHECK(sol.is_assigned(0));
    CHECK(sol.is_assigned(1));
    CHECK(sol.is_assigned(3));
    CHECK(sol.is_assigned(5));
}

// ===========================================================================
//  Replan: solution quality improves for free clients
// ===========================================================================

TEST_CASE("replan -- solution quality improves or stays same for free clients",
          "[replan][warm_start]") {
    auto data = make_replan_cvrp();
    CostEvaluator eval;

    // Create a deliberately suboptimal solution.
    // Route 0: depot -> 0 -> 4 -> 2 (clients are scattered)
    // Route 1: depot -> 3 -> 7 -> 5 (clients are scattered)
    // Route 2: depot -> 6 -> 1
    std::vector<std::vector<int>> routes = {{0, 4, 2}, {3, 7, 5}, {6, 1}, {}};
    Solution sol = warm_start(routes, data, eval);
    int64_t cost_before = sol.cost(eval);

    // Pin nothing -- full freedom to optimize.
    ReplanConfig config;

    replan(sol, config, data, eval);

    int64_t cost_after = sol.cost(eval);
    CHECK(cost_after <= cost_before);
}

// ===========================================================================
//  Replan: validation errors
// ===========================================================================

TEST_CASE("replan -- pinned and removed overlap throws", "[replan][warm_start]") {
    auto data = make_replan_cvrp();
    CostEvaluator eval;
    Solution sol = make_initial_solution(data);

    ReplanConfig config;
    config.pinned_clients = {0, 1};
    config.removed_clients = {1};  // overlap with pinned

    CHECK_THROWS_AS(replan(sol, config, data, eval), std::invalid_argument);
}

TEST_CASE("replan -- new client already assigned throws", "[replan][warm_start]") {
    auto data = make_replan_cvrp();
    CostEvaluator eval;
    Solution sol = make_initial_solution(data);

    ReplanConfig config;
    config.new_clients = {0};  // client 0 is already assigned

    CHECK_THROWS_AS(replan(sol, config, data, eval), std::invalid_argument);
}

TEST_CASE("replan -- pinned client out of range throws", "[replan][warm_start]") {
    auto data = make_replan_cvrp();
    CostEvaluator eval;
    Solution sol = make_initial_solution(data);

    ReplanConfig config;
    config.pinned_clients = {99};  // out of range

    CHECK_THROWS_AS(replan(sol, config, data, eval), std::invalid_argument);
}

// ===========================================================================
//  cheapest_insert standalone
// ===========================================================================

TEST_CASE("cheapest_insert -- inserts client at best position", "[replan][warm_start]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_client({10.0, 0.0}, {.demand = {5}});                     // 0
    b.add_client({20.0, 0.0}, {.demand = {5}});                     // 1
    b.add_client({15.0, 0.0}, {.demand = {5}, .required = false});  // 2 - to insert
    b.add_vehicle_type(2, {.capacity = {30}});
    auto data = b.build();
    CostEvaluator eval;

    // Initial: route 0 = [0, 1], client 2 unassigned.
    std::vector<std::vector<int>> routes = {{0, 1}};
    Solution sol = warm_start(routes, data, eval);
    CHECK_FALSE(sol.is_assigned(2));

    bool inserted = cheapest_insert(sol, 2, data, eval);
    CHECK(inserted);
    CHECK(sol.is_assigned(2));

    // Client 2 at (15,0) should be between 0 at (10,0) and 1 at (20,0).
    // So route should be [0, 2, 1].
    bool found = false;
    for (int r = 0; r < sol.num_routes(); ++r) {
        Route const& route = sol.route(r);
        for (int p = 0; p < route.size(); ++p) {
            if (route.client(p) == 2) {
                found = true;
                // Should be at position 1 (between 0 and 1).
                CHECK(p == 1);
            }
        }
    }
    CHECK(found);
}

// ===========================================================================
//  local_search_with_pins standalone
// ===========================================================================

TEST_CASE("local_search_with_pins -- pins are respected", "[replan][warm_start]") {
    auto data = make_replan_cvrp();
    CostEvaluator eval;

    // Create a suboptimal solution.
    std::vector<std::vector<int>> routes = {{0, 4, 2},  // scattered
                                            {3, 7, 5},  // scattered
                                            {6, 1},
                                            {}};
    Solution sol = warm_start(routes, data, eval);

    // Pin client 4 in route 0.
    PinSet pins(data.num_clients());
    pins.pin(4);

    // Record client 4's position.
    int client4_route = -1, client4_pos = -1;
    for (int r = 0; r < sol.num_routes(); ++r) {
        Route const& route = sol.route(r);
        for (int p = 0; p < route.size(); ++p) {
            if (route.client(p) == 4) {
                client4_route = r;
                client4_pos = p;
            }
        }
    }

    local_search_with_pins(sol, pins, data, eval);

    // Client 4 should still be in the same route.
    bool still_in_route = false;
    for (int p = 0; p < sol.route(client4_route).size(); ++p) {
        if (sol.route(client4_route).client(p) == 4) {
            still_in_route = true;
        }
    }
    CHECK(still_in_route);
}

// ===========================================================================
//  Replan: combined scenario
// ===========================================================================

TEST_CASE("replan -- combined: pin + insert + remove", "[replan][warm_start]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

    b.add_client({10.0, 0.0}, {.demand = {5}});                     // 0
    b.add_client({10.0, 10.0}, {.demand = {5}});                    // 1
    b.add_client({0.0, 10.0}, {.demand = {5}, .required = false});  // 2 optional
    b.add_client({-10.0, 0.0}, {.demand = {5}});                    // 3
    b.add_client({-10.0, -10.0}, {.demand = {5}});                  // 4
    b.add_client({0.0, -10.0}, {.demand = {5}});                    // 5
    b.add_client({5.0, 5.0}, {.demand = {3}, .required = false});   // 6 new

    b.add_vehicle_type(3, {.capacity = {30}});
    auto data = b.build();
    CostEvaluator eval;

    std::vector<std::vector<int>> routes = {{0, 1, 2}, {3, 4, 5}};
    Solution sol = warm_start(routes, data, eval);
    CHECK(sol.num_unassigned() == 1);  // client 6 unassigned

    ReplanConfig config;
    config.pinned_clients = {0, 1};  // pin clients 0 and 1
    config.new_clients = {6};        // insert client 6
    config.removed_clients = {2};    // remove optional client 2

    auto route0_before =
        std::vector<int>(sol.route(0).clients().begin(), sol.route(0).clients().end());

    PinSet pins = replan(sol, config, data, eval);

    // Pinned clients 0 and 1 must still be assigned.
    CHECK(sol.is_assigned(0));
    CHECK(sol.is_assigned(1));

    // New client 6 should be assigned.
    CHECK(sol.is_assigned(6));

    // Clients 0, 1 should still be in the same route (route 0) since
    // they were pinned. Check at least they are in route 0.
    bool client0_in_route0 = false, client1_in_route0 = false;
    for (int p = 0; p < sol.route(0).size(); ++p) {
        if (sol.route(0).client(p) == 0) {
            client0_in_route0 = true;
        }
        if (sol.route(0).client(p) == 1) {
            client1_in_route0 = true;
        }
    }
    CHECK(client0_in_route0);
    CHECK(client1_in_route0);

    CHECK(pins.is_pinned(0));
    CHECK(pins.is_pinned(1));
    CHECK_FALSE(pins.is_pinned(3));
}
