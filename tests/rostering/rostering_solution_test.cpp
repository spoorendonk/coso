#include "rostering/rostering_solution.h"

#include "rostering/cost_evaluator.h"
#include "rostering/rostering_data.h"

#include <catch2/catch_test_macros.hpp>

#include <climits>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small test instance
//    3 employees, 2 shift types (Day/Night), 7-day horizon
// ---------------------------------------------------------------------------

namespace {

RosteringData make_small_instance() {
    RosteringData data;

    // Shift types: Day (08-16, 8h) and Night (22-06, 8h).
    data.shift_types = {
        {.name = "Day", .duration_hours = 8},
        {.name = "Night", .duration_hours = 8},
    };

    // 3 employees with default constraints.
    data.employees = {
        {.name = "Alice", .skills = {"nurse"}, .max_consecutive_days = 5},
        {.name = "Bob", .skills = {"nurse"}, .max_consecutive_days = 5},
        {.name = "Carol", .skills = {"nurse", "senior"}, .max_consecutive_days = 5},
    };

    data.horizon = 7;

    // Demand: 1 nurse per day shift, 1 per night shift, every day.
    for (int d = 0; d < 7; ++d) {
        data.demand[RosteringData::demand_key(0, d)] = {
            .min_employees = 1, .max_employees = 2, .required_skill = ""};
        data.demand[RosteringData::demand_key(1, d)] = {
            .min_employees = 1, .max_employees = 1, .required_skill = ""};
    }

    return data;
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
//  Test: empty solution
// ---------------------------------------------------------------------------

TEST_CASE("RosteringSolution: empty solution", "[rostering]") {
    auto data = make_small_instance();
    RosteringCostEvaluator evaluator(data);
    RosteringSolution sol(data, evaluator);

    REQUIRE(sol.num_employees() == 3);
    REQUIRE(sol.horizon() == 7);

    // All cells should be -1 (unassigned).
    for (int e = 0; e < 3; ++e) {
        for (int d = 0; d < 7; ++d) {
            REQUIRE(sol.get(e, d) == -1);
        }
    }

    // Empty schedule has demand violations (understaffing).
    REQUIRE(sol.demand_cost() > 0);
}

// ---------------------------------------------------------------------------
//  Test: assign and unassign
// ---------------------------------------------------------------------------

TEST_CASE("RosteringSolution: assign and unassign", "[rostering]") {
    auto data = make_small_instance();
    RosteringCostEvaluator evaluator(data);
    RosteringSolution sol(data, evaluator);

    int cost_before = sol.cost();

    // Assign Alice to Day shift on day 0.
    int delta = sol.assign(0, 0, 0);
    REQUIRE(sol.get(0, 0) == 0);
    REQUIRE(sol.cost() == cost_before + delta);

    // Unassign.
    int cost_after_assign = sol.cost();
    int delta2 = sol.unassign(0, 0);
    REQUIRE(sol.get(0, 0) == -1);
    REQUIRE(sol.cost() == cost_after_assign + delta2);

    // After assign + unassign, cost should return to original.
    REQUIRE(sol.cost() == cost_before);
}

// ---------------------------------------------------------------------------
//  Test: demand violation computation
// ---------------------------------------------------------------------------

TEST_CASE("RosteringSolution: demand violations", "[rostering]") {
    auto data = make_small_instance();
    RosteringCostEvaluator evaluator(data);
    RosteringSolution sol(data, evaluator);

    // Empty schedule: 7 days * (1 understaffed day + 1 understaffed night)
    // = 14 understaffing violations.
    int empty_demand = sol.demand_cost();
    REQUIRE(empty_demand == 14 * evaluator.weights().understaffing);

    // Assign all day shifts to Alice and night shifts to Bob.
    for (int d = 0; d < 7; ++d) {
        sol.assign(0, d, 0);  // Alice -> Day
        sol.assign(1, d, 1);  // Bob -> Night
    }

    // All demands now satisfied: 0 demand cost.
    REQUIRE(sol.demand_cost() == 0);
}

// ---------------------------------------------------------------------------
//  Test: overstaffing penalty
// ---------------------------------------------------------------------------

TEST_CASE("RosteringSolution: overstaffing penalty", "[rostering]") {
    auto data = make_small_instance();
    RosteringCostEvaluator evaluator(data);
    RosteringSolution sol(data, evaluator);

    // Night demand max = 1.  Assign Alice and Bob to night on day 0.
    sol.assign(0, 0, 1);  // Alice -> Night day 0
    sol.assign(1, 0, 1);  // Bob -> Night day 0

    // Night shift on day 0 is overstaffed by 1.
    // Full demand cost includes understaffing for other days, but we can
    // verify the specific overstaffing by checking it is nonzero.
    int demand = sol.demand_cost();
    REQUIRE(demand > 0);

    // Remove overstaffing.
    sol.unassign(1, 0);
    int demand2 = sol.demand_cost();

    // Overstaffing removed: demand2 should be less than demand.
    REQUIRE(demand2 < demand);
}

// ---------------------------------------------------------------------------
//  Test: consecutive shift constraint
// ---------------------------------------------------------------------------

TEST_CASE("RosteringSolution: max consecutive shifts", "[rostering]") {
    auto data = make_small_instance();
    // Tighten: max 3 consecutive.
    data.employees[0].max_consecutive_days = 3;

    RosteringCostEvaluator evaluator(data);
    RosteringSolution sol(data, evaluator);

    // Assign Alice to Day shift 4 days in a row -> violation on day 4.
    for (int d = 0; d < 4; ++d) {
        sol.assign(0, d, 0);
    }

    REQUIRE(sol.hard_constraint_cost() > 0);
    REQUIRE_FALSE(sol.is_feasible());

    // Remove 4th day assignment -> should be feasible again (for that constraint).
    sol.unassign(0, 3);
    int consec_cost = evaluator.consecutive_violation_cost(sol.schedule());
    REQUIRE(consec_cost == 0);
}

// ---------------------------------------------------------------------------
//  Test: forbidden sequences
// ---------------------------------------------------------------------------

TEST_CASE("RosteringSolution: forbidden shift sequences", "[rostering]") {
    auto data = make_small_instance();
    // Forbid Night -> Day sequence.
    data.forbidden_sequences = {{1, 0}};

    RosteringCostEvaluator evaluator(data);
    RosteringSolution sol(data, evaluator);

    // Assign Night -> Day on consecutive days.
    sol.assign(0, 0, 1);  // Night
    sol.assign(0, 1, 0);  // Day
    REQUIRE(evaluator.forbidden_sequence_cost(sol.schedule()) > 0);

    // Day -> Night is OK.
    sol.unassign(0, 0);
    sol.unassign(0, 1);
    sol.assign(0, 0, 0);  // Day
    sol.assign(0, 1, 1);  // Night
    REQUIRE(evaluator.forbidden_sequence_cost(sol.schedule()) == 0);
}

// ---------------------------------------------------------------------------
//  Test: unavailability
// ---------------------------------------------------------------------------

TEST_CASE("RosteringSolution: unavailability", "[rostering]") {
    auto data = make_small_instance();
    // Alice is unavailable on day 3.
    data.unavailabilities.insert(RosteringData::unavail_key(0, 3));

    RosteringCostEvaluator evaluator(data);
    RosteringSolution sol(data, evaluator);

    // Assign Alice on day 3 -> violation.
    sol.assign(0, 3, 0);
    REQUIRE_FALSE(sol.is_feasible());

    // Remove the assignment -> feasible (wrt unavailability).
    sol.unassign(0, 3);
    REQUIRE(evaluator.unavailability_cost(sol.schedule()) == 0);
}

// ---------------------------------------------------------------------------
//  Test: preference costs
// ---------------------------------------------------------------------------

TEST_CASE("RosteringSolution: preference costs", "[rostering]") {
    auto data = make_small_instance();
    // Alice prefers Day shift on Monday (day 0), weight = 5.
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };

    RosteringCostEvaluator evaluator(data);
    RosteringSolution sol(data, evaluator);

    // Before assignment: no preference reward.
    REQUIRE(sol.preference_cost() == 0);

    // Assign Alice to Day on day 0 -> preference satisfied, cost = -5.
    sol.assign(0, 0, 0);
    REQUIRE(sol.preference_cost() == -5);

    // Assign to Night instead -> preference not met, cost = 0.
    sol.unassign(0, 0);
    sol.assign(0, 0, 1);
    REQUIRE(sol.preference_cost() == 0);
}

// ---------------------------------------------------------------------------
//  Test: swap operation
// ---------------------------------------------------------------------------

TEST_CASE("RosteringSolution: swap employees", "[rostering]") {
    auto data = make_small_instance();
    RosteringCostEvaluator evaluator(data);
    RosteringSolution sol(data, evaluator);

    sol.assign(0, 0, 0);  // Alice -> Day
    sol.assign(1, 0, 1);  // Bob -> Night

    int cost_before = sol.cost();
    int delta = sol.swap(0, 1, 0);
    REQUIRE(sol.cost() == cost_before + delta);

    // After swap: Alice -> Night, Bob -> Day.
    REQUIRE(sol.get(0, 0) == 1);
    REQUIRE(sol.get(1, 0) == 0);
}

// ---------------------------------------------------------------------------
//  Test: recompute_cost consistency
// ---------------------------------------------------------------------------

TEST_CASE("RosteringSolution: recompute_cost matches incremental", "[rostering]") {
    auto data = make_small_instance();
    RosteringCostEvaluator evaluator(data);
    RosteringSolution sol(data, evaluator);

    // Make several assignments.
    sol.assign(0, 0, 0);
    sol.assign(1, 0, 1);
    sol.assign(2, 1, 0);
    sol.assign(0, 2, 1);

    int incremental = sol.cost();
    sol.recompute_cost();
    REQUIRE(sol.cost() == incremental);
}

// ---------------------------------------------------------------------------
//  Test: skill-based demand
// ---------------------------------------------------------------------------

TEST_CASE("RosteringSolution: skill-based demand", "[rostering]") {
    auto data = make_small_instance();
    // Override day 0 day-shift demand to require "senior" skill.
    data.demand[RosteringData::demand_key(0, 0)] = {
        .min_employees = 1, .max_employees = 2, .required_skill = "senior"};

    RosteringCostEvaluator evaluator(data);
    RosteringSolution sol(data, evaluator);

    // Assign Alice (no senior skill) to Day on day 0 -> does not count.
    sol.assign(0, 0, 0);
    // Day 0 day-shift still understaffed for "senior".
    int demand_with_alice = evaluator.demand_cost(sol.schedule());

    // Assign Carol (has senior) to Day on day 0 -> should satisfy demand.
    sol.assign(2, 0, 0);
    int demand_with_carol = evaluator.demand_cost(sol.schedule());

    // demand should decrease when Carol is assigned.
    REQUIRE(demand_with_carol < demand_with_alice);
}
