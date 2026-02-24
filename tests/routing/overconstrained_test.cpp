#include <catch2/catch_test_macros.hpp>

#include "routing/overconstrained.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build test instances.
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), 4 clients, capacity 10, 2 vehicles.
static ProblemData make_basic_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {3}});   // client 0
    b.add_client({20.0, 0.0}, {.demand = {4}});   // client 1
    b.add_client({30.0, 0.0}, {.demand = {5}});   // client 2
    b.add_client({0.0, 10.0}, {.demand = {2}});   // client 3

    return b.build(0);
}

/// Instance that is overconstrained: total demand 24 but only 2 vehicles
/// with capacity 10 each (max feasible demand = 20).
static ProblemData make_overconstrained_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {7}});   // client 0
    b.add_client({20.0, 0.0}, {.demand = {8}});   // client 1
    b.add_client({30.0, 0.0}, {.demand = {6}});   // client 2
    b.add_client({0.0, 10.0}, {.demand = {3}});   // client 3

    return b.build(0);
}

/// Instance with time windows.
static ProblemData make_tw_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0}, {.tw = {0, 100}});
    b.add_vehicle_type(2, {.capacity = {100}});

    // Very tight TW that forces time warp.
    b.add_client({10.0, 0.0}, {.demand = {1}, .tw = {0, 5}});    // client 0
    b.add_client({20.0, 0.0}, {.demand = {1}, .tw = {0, 5}});    // client 1

    return b.build(0);
}

/// Instance with optional clients (required = false).
static ProblemData make_optional_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {5}});

    b.add_client({10.0, 0.0}, {.demand = {3}});                          // client 0: required
    b.add_client({20.0, 0.0}, {.demand = {4}});                          // client 1: required
    b.add_client({30.0, 0.0}, {.demand = {2}, .required = false});       // client 2: optional

    return b.build(0);
}

// ===========================================================================
//  num_unserved_required
// ===========================================================================

TEST_CASE("overconstrained: num_unserved_required with all served",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    // Serve all clients.
    sol.set_route_clients(0, {0, 1});
    sol.set_route_clients(1, {2, 3});

    CHECK(num_unserved_required(sol) == 0);
}

TEST_CASE("overconstrained: num_unserved_required with some unserved",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    // Only serve clients 0 and 1.
    sol.set_route_clients(0, {0, 1});

    CHECK(num_unserved_required(sol) == 2);  // clients 2 and 3 unserved
}

TEST_CASE("overconstrained: optional clients not counted as unserved required",
          "[overconstrained][routing]")
{
    auto data = make_optional_instance();
    Solution sol(data);

    // Serve client 0 only.  Client 1 (required) and 2 (optional) unserved.
    sol.set_route_clients(0, {0});

    CHECK(sol.num_unassigned() == 2);
    CHECK(num_unserved_required(sol) == 1);  // Only client 1 is required
}

// ===========================================================================
//  total_load_excess
// ===========================================================================

TEST_CASE("overconstrained: total_load_excess with feasible solution",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    sol.set_route_clients(0, {0, 1});  // load = 7, cap = 10
    sol.set_route_clients(1, {2, 3});  // load = 7, cap = 10

    CHECK(total_load_excess(sol) == 0);
}

TEST_CASE("overconstrained: total_load_excess with overloaded routes",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    // Route 0: clients 0,1,2 -> load = 12, cap = 10, excess = 2
    sol.set_route_clients(0, {0, 1, 2});
    sol.set_route_clients(1, {3});  // load = 2, feasible

    CHECK(total_load_excess(sol) == 2);
}

// ===========================================================================
//  total_time_warp
// ===========================================================================

TEST_CASE("overconstrained: total_time_warp with tight time windows",
          "[overconstrained][routing]")
{
    auto data = make_tw_instance();
    Solution sol(data);

    // Both clients on one route: depot(0,0) -> c0(10,0) -> c1(20,0) -> depot
    // Arrive at c0 at time 10, TW ends at 5 -> time warp = 5.
    // Arrive at c1 at time 20, TW ends at 5 -> additional time warp.
    sol.set_route_clients(0, {0, 1});

    CHECK(total_time_warp(sol) > 0);
}

// ===========================================================================
//  overconstrained_penalty
// ===========================================================================

TEST_CASE("overconstrained: penalty is zero for feasible solution",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    sol.set_route_clients(0, {0, 1});
    sol.set_route_clients(1, {2, 3});

    OverconstrainedConfig config;
    CHECK(overconstrained_penalty(sol, config) == 0);
}

TEST_CASE("overconstrained: penalty for unserved clients",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    // Only serve 2 of 4 clients.
    sol.set_route_clients(0, {0, 1});

    OverconstrainedConfig config;
    config.allow_unserved = true;
    config.unserved_penalty = 5000;
    config.capacity_violation_penalty = 0;
    config.tw_violation_penalty = 0;

    // 2 unserved required clients * 5000 = 10000
    CHECK(overconstrained_penalty(sol, config) == 10000);
}

TEST_CASE("overconstrained: penalty for capacity violation",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    // Overload route 0.
    sol.set_route_clients(0, {0, 1, 2});  // load = 12, excess = 2
    sol.set_route_clients(1, {3});

    OverconstrainedConfig config;
    config.allow_unserved = false;
    config.capacity_violation_penalty = 500;
    config.tw_violation_penalty = 0;

    // 2 excess * 500 = 1000
    CHECK(overconstrained_penalty(sol, config) == 1000);
}

TEST_CASE("overconstrained: combined penalties accumulate",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    // Only serve 3 clients, with overload on route 0.
    sol.set_route_clients(0, {0, 1, 2});  // load = 12, excess = 2

    OverconstrainedConfig config;
    config.allow_unserved = true;
    config.unserved_penalty = 1000;
    config.capacity_violation_penalty = 500;
    config.tw_violation_penalty = 0;

    // 1 unserved (client 3) * 1000 + 2 excess * 500 = 2000
    CHECK(overconstrained_penalty(sol, config) == 2000);
}

// ===========================================================================
//  overconstrained_cost
// ===========================================================================

TEST_CASE("overconstrained: cost includes objective and penalties",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    sol.set_route_clients(0, {0, 1});
    sol.set_route_clients(1, {2, 3});

    OverconstrainedConfig config;
    config.allow_unserved = false;

    // Feasible solution: overconstrained cost = pure objective.
    CostEvaluator zero_eval(0, 0, 0);
    int64_t obj = sol.objective(zero_eval);

    CHECK(overconstrained_cost(sol, config) == obj);
}

TEST_CASE("overconstrained: cost with penalties exceeds base objective",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    sol.set_route_clients(0, {0, 1, 2});  // overloaded
    sol.set_route_clients(1, {3});

    OverconstrainedConfig config;
    config.capacity_violation_penalty = 500;

    CostEvaluator zero_eval(0, 0, 0);
    int64_t obj = sol.objective(zero_eval);
    int64_t oc_cost = overconstrained_cost(sol, config);

    CHECK(oc_cost > obj);
    CHECK(oc_cost == obj + overconstrained_penalty(sol, config));
}

// ===========================================================================
//  overconstrained_feasible
// ===========================================================================

TEST_CASE("overconstrained: feasible when all served and no violations",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    sol.set_route_clients(0, {0, 1});
    sol.set_route_clients(1, {2, 3});

    OverconstrainedConfig config;
    CHECK(overconstrained_feasible(sol, config));
}

TEST_CASE("overconstrained: infeasible when required clients unserved",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    sol.set_route_clients(0, {0, 1});

    OverconstrainedConfig config;
    config.allow_unserved = false;

    CHECK_FALSE(overconstrained_feasible(sol, config));
}

TEST_CASE("overconstrained: feasible with allow_unserved even when clients unserved",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    sol.set_route_clients(0, {0, 1});

    OverconstrainedConfig config;
    config.allow_unserved = true;

    // Still infeasible due to load/tw checking of routes.
    // Routes are feasible here (load 7 <= 10).
    CHECK(overconstrained_feasible(sol, config));
}

TEST_CASE("overconstrained: infeasible when routes are overloaded",
          "[overconstrained][routing]")
{
    auto data = make_basic_instance();
    Solution sol(data);

    sol.set_route_clients(0, {0, 1, 2});  // load = 12 > 10
    sol.set_route_clients(1, {3});

    OverconstrainedConfig config;
    config.allow_unserved = false;

    CHECK_FALSE(overconstrained_feasible(sol, config));
}

// ===========================================================================
//  OverconstrainedConfig defaults
// ===========================================================================

TEST_CASE("OverconstrainedConfig: default values", "[overconstrained][routing]")
{
    OverconstrainedConfig config;

    CHECK_FALSE(config.allow_unserved);
    CHECK(config.unserved_penalty == 10000);
    CHECK(config.tw_violation_penalty == 1000);
    CHECK(config.capacity_violation_penalty == 1000);
}
