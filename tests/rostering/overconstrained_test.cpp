#include "rostering/overconstrained.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build test instances.
// ---------------------------------------------------------------------------

/// 3 employees, 2 shift types, 5-day horizon, demand of 1 per shift per day.
/// max_consecutive_shifts = 5 (no consecutive violation possible in 5 days).
static RosteringData make_basic_data() {
    RosteringData data;
    data.horizon = 5;

    data.shift_types.push_back({.name = "Day", .start_hour = 8, .end_hour = 16});
    data.shift_types.push_back({.name = "Night", .start_hour = 22, .end_hour = 6});

    data.employees.push_back({.name = "Alice"});
    data.employees.push_back({.name = "Bob"});
    data.employees.push_back({.name = "Carol"});

    // Demand: 1 employee per shift per day.
    for (int s = 0; s < 2; ++s) {
        for (int d = 0; d < 5; ++d) {
            data.demand[RosteringData::demand_key(s, d)] = {.min_employees = 1,
                                                             .max_employees = 2};
        }
    }

    data.max_consecutive_shifts = 5;

    return data;
}

/// Overconstrained: 2 employees but demand of 2 per shift (needs 4).
static RosteringData make_overconstrained_data() {
    RosteringData data;
    data.horizon = 3;

    data.shift_types.push_back({.name = "Day", .start_hour = 8, .end_hour = 16});

    data.employees.push_back({.name = "Alice"});
    data.employees.push_back({.name = "Bob"});

    // Demand: 2 employees per day shift, but only 2 employees total.
    for (int d = 0; d < 3; ++d) {
        data.demand[RosteringData::demand_key(0, d)] = {.min_employees = 2, .max_employees = 3};
    }

    return data;
}

/// Create an empty schedule for the given data.
static std::vector<std::vector<int>> make_empty_schedule(RosteringData const& data) {
    return std::vector<std::vector<int>>(data.num_employees(), std::vector<int>(data.horizon, -1));
}

// ===========================================================================
//  rostering_total_understaffing
// ===========================================================================

TEST_CASE("assignment overconstrained: no understaffing when demand met",
          "[overconstrained][rostering]") {
    auto data = make_basic_data();
    auto sched = make_empty_schedule(data);

    // Assign 1 employee per shift per day (meets min demand of 1).
    for (int d = 0; d < 5; ++d) {
        sched[0][d] = 0;  // Alice -> Day
        sched[1][d] = 1;  // Bob -> Night
    }

    CHECK(rostering_total_understaffing(data, sched) == 0);
}

TEST_CASE("assignment overconstrained: understaffing when demand unmet",
          "[overconstrained][rostering]") {
    auto data = make_basic_data();
    auto sched = make_empty_schedule(data);

    // Nobody assigned -> all shifts understaffed.
    // 2 shifts * 5 days * 1 shortfall = 10
    CHECK(rostering_total_understaffing(data, sched) == 10);
}

TEST_CASE("assignment overconstrained: partial understaffing", "[overconstrained][rostering]") {
    auto data = make_overconstrained_data();
    auto sched = make_empty_schedule(data);

    // Both employees work all 3 days.
    for (int d = 0; d < 3; ++d) {
        sched[0][d] = 0;
        sched[1][d] = 0;
    }

    // Demand is 2, we have 2 -> no understaffing.
    CHECK(rostering_total_understaffing(data, sched) == 0);
}

TEST_CASE("assignment overconstrained: understaffing with partial coverage",
          "[overconstrained][rostering]") {
    auto data = make_overconstrained_data();
    auto sched = make_empty_schedule(data);

    // Only 1 of 2 employees works each day.
    for (int d = 0; d < 3; ++d) {
        sched[0][d] = 0;
        // sched[1] is off
    }

    // Demand is 2, only 1 working -> 1 shortfall * 3 days = 3.
    CHECK(rostering_total_understaffing(data, sched) == 3);
}

// ===========================================================================
//  rostering_total_hard_violations
// ===========================================================================

TEST_CASE("assignment overconstrained: no violations with valid schedule",
          "[overconstrained][rostering]") {
    auto data = make_basic_data();
    RosteringCostEvaluator eval(data);
    auto sched = make_empty_schedule(data);

    // 5-day horizon, max_consecutive = 5 -> working all 5 days is fine.
    for (int d = 0; d < 5; ++d) {
        sched[0][d] = 0;  // Alice -> Day
        sched[1][d] = 1;  // Bob -> Night
    }

    CHECK(rostering_total_hard_violations(data, eval, sched) == 0);
}

TEST_CASE("assignment overconstrained: consecutive violations detected",
          "[overconstrained][rostering]") {
    auto data = make_basic_data();
    data.max_consecutive_shifts = 3;

    RosteringCostEvaluator eval(data);
    auto sched = make_empty_schedule(data);

    // Alice works all 5 days -> max_consecutive = 3, violations on days 4-5.
    for (int d = 0; d < 5; ++d) {
        sched[0][d] = 0;
    }

    CHECK(rostering_total_hard_violations(data, eval, sched) > 0);
}

// ===========================================================================
//  rostering_overconstrained_penalty
// ===========================================================================

TEST_CASE("assignment overconstrained: penalty is zero when all satisfied",
          "[overconstrained][rostering]") {
    auto data = make_basic_data();
    RosteringCostEvaluator eval(data);
    auto sched = make_empty_schedule(data);

    // Satisfy all demand for 5-day horizon (no consecutive violation).
    for (int d = 0; d < 5; ++d) {
        sched[0][d] = 0;  // Alice -> Day
        sched[1][d] = 1;  // Bob -> Night
    }

    RosteringOverconstrainedConfig config;
    CHECK(rostering_overconstrained_penalty(data, eval, sched, config) == 0);
}

TEST_CASE("assignment overconstrained: penalty for understaffing",
          "[overconstrained][rostering]") {
    auto data = make_basic_data();
    RosteringCostEvaluator eval(data);
    auto sched = make_empty_schedule(data);

    // Empty schedule -> all understaffed.
    RosteringOverconstrainedConfig config;
    config.understaffing_penalty = 100;
    config.constraint_violation_penalty = 0;

    // 10 total understaffing * 100 = 1000
    CHECK(rostering_overconstrained_penalty(data, eval, sched, config) == 1000);
}

// ===========================================================================
//  rostering_overconstrained_cost
// ===========================================================================

TEST_CASE("assignment overconstrained: cost includes preferences and penalties",
          "[overconstrained][rostering]") {
    auto data = make_basic_data();
    // Add a preference: Alice prefers Day shift on day 0, weight 10.
    data.preferences.push_back({.employee = 0, .day = 0, .shift_type = 0, .weight = 10});

    RosteringCostEvaluator eval(data);
    auto sched = make_empty_schedule(data);

    // Satisfy demand (5-day horizon).
    for (int d = 0; d < 5; ++d) {
        sched[0][d] = 0;  // Alice -> Day (satisfies preference on day 0)
        sched[1][d] = 1;  // Bob -> Night
    }

    RosteringOverconstrainedConfig config;
    int64_t cost = rostering_overconstrained_cost(data, eval, sched, config);

    // Preference cost is negative (reward): -10 * 1 = -10.
    // No understaffing or violations.
    CHECK(cost == -10);
}

// ===========================================================================
//  rostering_overconstrained_feasible
// ===========================================================================

TEST_CASE("assignment overconstrained: feasible when no violations",
          "[overconstrained][rostering]") {
    auto data = make_basic_data();
    RosteringCostEvaluator eval(data);
    auto sched = make_empty_schedule(data);

    // Valid schedule (5-day horizon, no hard violations).
    for (int d = 0; d < 5; ++d) {
        sched[0][d] = 0;
        sched[1][d] = 1;
    }

    CHECK(rostering_overconstrained_feasible(eval, sched));
}

// ===========================================================================
//  RosteringOverconstrainedConfig defaults
// ===========================================================================

TEST_CASE("RosteringOverconstrainedConfig: default values", "[overconstrained][rostering]") {
    RosteringOverconstrainedConfig config;

    CHECK(config.understaffing_penalty == 10000);
    CHECK(config.constraint_violation_penalty == 5000);
}
