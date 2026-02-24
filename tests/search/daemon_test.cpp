#include <catch2/catch_test_macros.hpp>

#include "search/daemon.h"
#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/stop_criterion.h"

#include <chrono>
#include <thread>

using namespace coso;

// --------------------------------------------------------------------------- //
//  Helper: build small CVRP instances for testing.                             //
// --------------------------------------------------------------------------- //

/// 1 depot at (0,0), 6 clients in a line, 2 vehicles with capacity 20.
static ProblemData make_small_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {20}});

    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {4}});
    b.add_client({30.0, 0.0}, {.demand = {5}});
    b.add_client({40.0, 0.0}, {.demand = {2}});
    b.add_client({50.0, 0.0}, {.demand = {1}});
    b.add_client({60.0, 0.0}, {.demand = {3}});

    return b.build(0);
}

/// Build a modified version of the small instance with an extra client.
static ProblemData make_modified_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {20}});

    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {4}});
    b.add_client({30.0, 0.0}, {.demand = {5}});
    b.add_client({40.0, 0.0}, {.demand = {2}});
    b.add_client({50.0, 0.0}, {.demand = {1}});
    b.add_client({60.0, 0.0}, {.demand = {3}});
    b.add_client({70.0, 0.0}, {.demand = {2}});  // extra client

    return b.build(0);
}

// =========================================================================== //
//  Start/stop lifecycle                                                        //
// =========================================================================== //

TEST_CASE("Daemon: start and stop without error",
          "[search][daemon]")
{
    auto data = make_small_instance();
    CostEvaluator eval;
    StopCriterion stop(2.0);  // 2 second time limit

    Daemon daemon(data);

    CHECK_FALSE(daemon.running());

    daemon.start(eval, stop);
    CHECK(daemon.running());

    // Let it run briefly.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    daemon.stop();
    CHECK_FALSE(daemon.running());
}

TEST_CASE("Daemon: destructor stops the solver",
          "[search][daemon]")
{
    auto data = make_small_instance();
    CostEvaluator eval;
    StopCriterion stop(5.0);

    {
        Daemon daemon(data);
        daemon.start(eval, stop);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // Destructor should stop without hanging.
    }
}

TEST_CASE("Daemon: double start is safe",
          "[search][daemon]")
{
    auto data = make_small_instance();
    CostEvaluator eval;
    StopCriterion stop(2.0);

    Daemon daemon(data);
    daemon.start(eval, stop);
    daemon.start(eval, stop);  // should be a no-op

    CHECK(daemon.running());

    daemon.stop();
}

// =========================================================================== //
//  current_solution returns valid results                                      //
// =========================================================================== //

TEST_CASE("Daemon: current_solution is nullopt before start",
          "[search][daemon]")
{
    auto data = make_small_instance();
    Daemon daemon(data);

    auto sol = daemon.current_solution();
    CHECK_FALSE(sol.has_value());
}

TEST_CASE("Daemon: current_solution returns a valid solution after running",
          "[search][daemon]")
{
    auto data = make_small_instance();
    CostEvaluator eval;
    StopCriterion stop(0.0, 200, 0);  // iteration-limited

    Daemon daemon(data);
    daemon.start(eval, stop);

    // Wait for the solver to finish (iteration-limited, should be fast).
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    daemon.stop();

    auto sol = daemon.current_solution();
    REQUIRE(sol.has_value());
    CHECK(sol->num_unassigned() == 0);
}

// =========================================================================== //
//  current_solution is available while still running                           //
// =========================================================================== //

TEST_CASE("Daemon: current_solution available during execution",
          "[search][daemon]")
{
    auto data = make_small_instance();
    CostEvaluator eval;
    StopCriterion stop(3.0);

    Daemon daemon(data);
    daemon.start(eval, stop);

    // Poll until a solution appears (with timeout).
    std::optional<Solution> sol;
    for (int i = 0; i < 200; ++i) {
        sol = daemon.current_solution();
        if (sol.has_value())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    daemon.stop();

    REQUIRE(sol.has_value());
    CHECK(sol->num_unassigned() == 0);
}

// =========================================================================== //
//  Updates are applied                                                         //
// =========================================================================== //

TEST_CASE("Daemon: update changes the problem data",
          "[search][daemon]")
{
    auto data = make_small_instance();
    CostEvaluator eval;
    StopCriterion stop(3.0);

    Daemon daemon(data);
    daemon.start(eval, stop);

    // Wait for initial solution.
    for (int i = 0; i < 200; ++i) {
        if (daemon.current_solution().has_value())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(daemon.current_solution().has_value());

    // The original instance has 6 clients.
    CHECK(daemon.current_solution()->num_unassigned() == 0);

    // Apply an update that adds a 7th client.
    daemon.update([](ProblemData const& /*old_data*/) {
        return make_modified_instance();
    });

    // Wait for a new solution after update.
    std::optional<Solution> sol;
    for (int i = 0; i < 200; ++i) {
        sol = daemon.current_solution();
        if (sol.has_value())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    daemon.stop();

    // After update, the problem has 7 clients.
    REQUIRE(sol.has_value());

    // Count total assigned + unassigned -- should be 7.
    int total = sol->num_unassigned();
    for (int v = 0; v < sol->num_routes(); ++v) {
        total += sol->route(v).size();
    }
    CHECK(total == 7);
}

// =========================================================================== //
//  Stop criterion triggers automatically                                       //
// =========================================================================== //

TEST_CASE("Daemon: stops when stop criterion triggers",
          "[search][daemon]")
{
    auto data = make_small_instance();
    CostEvaluator eval;
    StopCriterion stop(0.0, 100, 0);  // 100 iterations max

    Daemon daemon(data);
    daemon.start(eval, stop);

    // Wait for the solver to finish naturally.
    auto start_time = std::chrono::steady_clock::now();
    while (daemon.running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > std::chrono::seconds(5))
            break;  // safety timeout
    }

    CHECK_FALSE(daemon.running());

    auto sol = daemon.current_solution();
    REQUIRE(sol.has_value());
}

// =========================================================================== //
//  Update on stopped daemon is a no-op                                         //
// =========================================================================== //

TEST_CASE("Daemon: update on stopped daemon is safe",
          "[search][daemon]")
{
    auto data = make_small_instance();

    Daemon daemon(data);
    // Not started -- update should be a no-op.
    daemon.update([](ProblemData const& old) { return old; });

    CHECK_FALSE(daemon.running());
}
