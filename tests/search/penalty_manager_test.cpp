#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "search/penalty_manager.h"

using namespace coso;
using Catch::Matchers::WithinAbs;

TEST_CASE("PenaltyManager default construction", "[search][penalty_manager]")
{
    PenaltyManager pm;

    CHECK(pm.load_penalty() == 100.0);
    CHECK(pm.tw_penalty() == 100.0);
    CHECK(pm.dist_penalty() == 100.0);
    CHECK(pm.num_registered() == 0);
}

TEST_CASE("PenaltyManager cost_evaluator returns current weights",
          "[search][penalty_manager]")
{
    PenaltyManager pm;
    auto eval = pm.cost_evaluator();

    CHECK(eval.load_penalty() == 100);
    CHECK(eval.tw_penalty() == 100);
    CHECK(eval.dist_penalty() == 100);
}

TEST_CASE("Penalties decrease when all solutions are feasible",
          "[search][penalty_manager]")
{
    // target = 0.5, rate = 0.1, update every 10 registrations.
    PenaltyManager pm(0.5, 0.1, 10);

    double initial = pm.load_penalty();

    // Register 10 feasible solutions -> frac = 1.0 > 0.5 -> decrease.
    for (int i = 0; i < 10; ++i)
        pm.register_solution(true);

    CHECK(pm.load_penalty() < initial);
    CHECK_THAT(pm.load_penalty(), WithinAbs(initial * 0.9, 1e-9));
    CHECK_THAT(pm.tw_penalty(), WithinAbs(initial * 0.9, 1e-9));
}

TEST_CASE("Penalties increase when no solutions are feasible",
          "[search][penalty_manager]")
{
    PenaltyManager pm(0.5, 0.1, 10);

    double initial = pm.load_penalty();

    // Register 10 infeasible solutions -> frac = 0.0 < 0.5 -> increase.
    for (int i = 0; i < 10; ++i)
        pm.register_solution(false);

    CHECK(pm.load_penalty() > initial);
    CHECK_THAT(pm.load_penalty(), WithinAbs(initial * 1.1, 1e-9));
}

TEST_CASE("Penalties unchanged when feasibility matches target",
          "[search][penalty_manager]")
{
    PenaltyManager pm(0.5, 0.1, 10);

    double initial = pm.load_penalty();

    // Register exactly 50% feasible -> no change.
    for (int i = 0; i < 10; ++i)
        pm.register_solution(i < 5);

    CHECK_THAT(pm.load_penalty(), WithinAbs(initial, 1e-9));
}

TEST_CASE("No update before interval is reached",
          "[search][penalty_manager]")
{
    PenaltyManager pm(0.5, 0.1, 100);

    double initial = pm.load_penalty();

    // Register 99 infeasible solutions: no update yet.
    for (int i = 0; i < 99; ++i)
        pm.register_solution(false);

    CHECK(pm.load_penalty() == initial);
    CHECK(pm.num_registered() == 99);
}

TEST_CASE("Multiple update cycles accumulate",
          "[search][penalty_manager]")
{
    PenaltyManager pm(0.5, 0.1, 10);

    double initial = pm.load_penalty();

    // 3 cycles of all-feasible -> penalty should decrease 3 times.
    for (int cycle = 0; cycle < 3; ++cycle)
        for (int i = 0; i < 10; ++i)
            pm.register_solution(true);

    double expected = initial * 0.9 * 0.9 * 0.9;
    CHECK_THAT(pm.load_penalty(), WithinAbs(expected, 1e-6));
    CHECK(pm.num_registered() == 30);
}

TEST_CASE("Penalties are clamped to minimum",
          "[search][penalty_manager]")
{
    // Large rate to drive penalties down fast.
    PenaltyManager pm(0.5, 0.99, 1);

    // Many feasible solutions -> penalties decrease rapidly.
    for (int i = 0; i < 1000; ++i)
        pm.register_solution(true);

    CHECK(pm.load_penalty() >= 1.0);
    CHECK(pm.tw_penalty() >= 1.0);
    CHECK(pm.dist_penalty() >= 1.0);
}

TEST_CASE("Penalties are clamped to maximum",
          "[search][penalty_manager]")
{
    // Large rate to drive penalties up fast.
    PenaltyManager pm(0.5, 0.99, 1);

    // Many infeasible solutions -> penalties increase rapidly.
    for (int i = 0; i < 1000; ++i)
        pm.register_solution(false);

    CHECK(pm.load_penalty() <= 100000.0);
    CHECK(pm.tw_penalty() <= 100000.0);
    CHECK(pm.dist_penalty() <= 100000.0);
}

TEST_CASE("feasible_fraction tracks current window",
          "[search][penalty_manager]")
{
    PenaltyManager pm(0.5, 0.1, 10);

    pm.register_solution(true);
    pm.register_solution(false);
    pm.register_solution(true);

    CHECK_THAT(pm.feasible_fraction(), WithinAbs(2.0 / 3.0, 1e-9));
}

TEST_CASE("Window resets after update",
          "[search][penalty_manager]")
{
    PenaltyManager pm(0.5, 0.1, 4);

    // Fill one window: 3 feasible, 1 infeasible.
    pm.register_solution(true);
    pm.register_solution(true);
    pm.register_solution(true);
    pm.register_solution(false);  // triggers update, window resets

    // New window: 1 infeasible registered.
    pm.register_solution(false);
    CHECK_THAT(pm.feasible_fraction(), WithinAbs(0.0, 1e-9));
}
