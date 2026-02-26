#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "search/stop_criterion.h"

#include <thread>

using namespace coso;

TEST_CASE("StopCriterion — time limit only", "[stop_criterion]")
{
    StopCriterion stop(0.1);  // 100ms

    CHECK_FALSE(stop.should_stop());
    CHECK(stop.iterations() == 0);
    CHECK(stop.iterations_no_improve() == 0);

    // Busy-wait until time expires.
    while (!stop.should_stop()) {
        stop.iteration();
    }

    CHECK(stop.should_stop());
    CHECK(stop.elapsed() >= 0.1);
    CHECK(stop.iterations() > 0);
}

TEST_CASE("StopCriterion — max iterations", "[stop_criterion]")
{
    StopCriterion stop(0.0, 10, 0);  // no time limit, 10 iters

    for (int i = 0; i < 10; ++i) {
        CHECK_FALSE(stop.should_stop());
        stop.iteration();
    }

    CHECK(stop.should_stop());
    CHECK(stop.iterations() == 10);
}

TEST_CASE("StopCriterion — max no improve", "[stop_criterion]")
{
    StopCriterion stop(0.0, 0, 5);  // 5 iterations without improvement

    // 3 iterations, then improve.
    for (int i = 0; i < 3; ++i) {
        stop.iteration();
    }
    CHECK_FALSE(stop.should_stop());
    stop.improvement();  // reset no-improve counter

    // 4 more iterations without improvement (total no-improve = 4).
    for (int i = 0; i < 4; ++i) {
        stop.iteration();
        CHECK_FALSE(stop.should_stop());
    }

    // 5th iteration without improvement => should stop.
    stop.iteration();
    CHECK(stop.should_stop());
    CHECK(stop.iterations_no_improve() == 5);
}

TEST_CASE("StopCriterion — combined criteria", "[stop_criterion]")
{
    // Time limit 10s (won't trigger), max iter 20, no improve 8.
    StopCriterion stop(10.0, 20, 8);

    // Improve at iteration 5.
    for (int i = 0; i < 5; ++i) {
        stop.iteration();
    }
    stop.improvement();

    // 8 more without improvement => should stop at iteration 13.
    for (int i = 0; i < 7; ++i) {
        stop.iteration();
        CHECK_FALSE(stop.should_stop());
    }
    stop.iteration();  // iteration 13, 8 without improve
    CHECK(stop.should_stop());
    CHECK(stop.iterations() == 13);
}

TEST_CASE("StopCriterion — no criteria active", "[stop_criterion]")
{
    StopCriterion stop(0.0, 0, 0);  // all disabled

    for (int i = 0; i < 100; ++i) {
        CHECK_FALSE(stop.should_stop());
        stop.iteration();
    }
    // Still won't stop — caller must have at least one criterion.
    CHECK_FALSE(stop.should_stop());
}

TEST_CASE("StopCriterion — deterministic work limit", "[stop_criterion]")
{
    StopCriterion stop(0.0, 0, 0);
    WorkUnits work;
    stop.set_work_limit(&work, 10);

    CHECK_FALSE(stop.should_stop());
    work.count(9);
    CHECK_FALSE(stop.should_stop());

    work.count(1);
    CHECK(stop.should_stop());
    CHECK(stop.work_ticks() == 10);
    CHECK(stop.work_units() > 0.0);
}

TEST_CASE("StopCriterion — elapsed time increases", "[stop_criterion]")
{
    StopCriterion stop(10.0);
    double t0 = stop.elapsed();
    CHECK(t0 >= 0.0);

    // Burn some time.
    volatile int sink = 0;
    for (int i = 0; i < 1'000'000; ++i)
        sink += i;

    double t1 = stop.elapsed();
    CHECK(t1 >= t0);
}
