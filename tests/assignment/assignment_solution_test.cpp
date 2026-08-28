#include "assignment/assignment_solution.h"

#include "assignment/assignment_data.h"
#include "assignment/cost_evaluator.h"

#include <catch2/catch_test_macros.hpp>

#include <climits>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small test instance
//    3 employees, 2 shift types (Day/Night), 7-day horizon
// ---------------------------------------------------------------------------

namespace {

AssignmentData make_small_instance() {
    AssignmentData data;

    // Shift types: Day (08-16, 8h) and Night (22-06, 8h).
    data.shift_types = {
        {.name = "Day", .start_hour = 8, .end_hour = 16, .duration_hours = 0},
        {.name = "Night", .start_hour = 22, .end_hour = 6, .duration_hours = 0},
    };

    // 3 employees with default constraints.
    data.employees = {
        {.name = "Alice",
         .skills = {"nurse"},
         .max_hours_per_week = 40,
         .max_consecutive_days = 5,
         .min_rest_hours = 11},
        {.name = "Bob",
         .skills = {"nurse"},
         .max_hours_per_week = 40,
         .max_consecutive_days = 5,
         .min_rest_hours = 11},
        {.name = "Carol",
         .skills = {"nurse", "senior"},
         .max_hours_per_week = 40,
         .max_consecutive_days = 5,
         .min_rest_hours = 11},
    };

    data.horizon = 7;

    // Demand: 1 nurse per day shift, 1 per night shift, every day.
    for (int d = 0; d < 7; ++d) {
        data.demand[AssignmentData::demand_key(0, d)] = {
            .min_employees = 1, .max_employees = 2, .required_skill = ""};
        data.demand[AssignmentData::demand_key(1, d)] = {
            .min_employees = 1, .max_employees = 1, .required_skill = ""};
    }

    // Global hard constraints.
    data.max_consecutive_shifts = 5;
    data.min_rest_between_shifts = 11;

    return data;
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
//  Test: empty solution
// ---------------------------------------------------------------------------

TEST_CASE("AssignmentSolution: empty solution", "[assignment]") {
    auto data = make_small_instance();
    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution sol(data, evaluator);

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

TEST_CASE("AssignmentSolution: assign and unassign", "[assignment]") {
    auto data = make_small_instance();
    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution sol(data, evaluator);

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

TEST_CASE("AssignmentSolution: demand violations", "[assignment]") {
    auto data = make_small_instance();
    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution sol(data, evaluator);

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

TEST_CASE("AssignmentSolution: overstaffing penalty", "[assignment]") {
    auto data = make_small_instance();
    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution sol(data, evaluator);

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

TEST_CASE("AssignmentSolution: max consecutive shifts", "[assignment]") {
    auto data = make_small_instance();
    // Tighten: max 3 consecutive.
    data.max_consecutive_shifts = 3;
    data.employees[0].max_consecutive_days = 3;

    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution sol(data, evaluator);

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
//  Test: minimum rest between shifts
// ---------------------------------------------------------------------------

TEST_CASE("AssignmentSolution: min rest between shifts", "[assignment]") {
    auto data = make_small_instance();
    data.min_rest_between_shifts = 11;

    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution sol(data, evaluator);

    // Day shift ends at 16:00, Night shift starts at 22:00.
    // Rest = (24 - 16) + 22 = 30 hours -> OK.
    sol.assign(0, 0, 0);  // Alice Day on day 0
    sol.assign(0, 1, 1);  // Alice Night on day 1
    REQUIRE(evaluator.rest_violation_cost(sol.schedule()) == 0);

    // Night shift ends at 06:00, Day shift starts at 08:00.
    // Rest = (24 - 6) + 8 = 26 hours -> OK.
    sol.unassign(0, 0);
    sol.unassign(0, 1);
    sol.assign(0, 0, 1);  // Alice Night on day 0
    sol.assign(0, 1, 0);  // Alice Day on day 1
    REQUIRE(evaluator.rest_violation_cost(sol.schedule()) == 0);

    // Create a tight scenario: shift ending at 20, next starting at 6.
    // Rest = (24 - 20) + 6 = 10 hours < 11 -> violation.
    AssignmentData data2;
    data2.shift_types = {
        {.name = "Late", .start_hour = 12, .end_hour = 20, .duration_hours = 0},
        {.name = "Early", .start_hour = 6, .end_hour = 14, .duration_hours = 0},
    };
    data2.employees = {{.name = "X",
                        .skills = {},
                        .max_hours_per_week = 40,
                        .max_consecutive_days = 7,
                        .min_rest_hours = 11}};
    data2.horizon = 2;
    data2.min_rest_between_shifts = 11;

    AssignmentCostEvaluator eval2(data2);
    AssignmentSolution sol2(data2, eval2);
    sol2.assign(0, 0, 0);  // Late on day 0
    sol2.assign(0, 1, 1);  // Early on day 1
    REQUIRE(eval2.rest_violation_cost(sol2.schedule()) > 0);
}

// ---------------------------------------------------------------------------
//  Test: forbidden sequences
// ---------------------------------------------------------------------------

TEST_CASE("AssignmentSolution: forbidden shift sequences", "[assignment]") {
    auto data = make_small_instance();
    // Forbid Night -> Day sequence.
    data.forbidden_sequences = {{1, 0}};

    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution sol(data, evaluator);

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

TEST_CASE("AssignmentSolution: unavailability", "[assignment]") {
    auto data = make_small_instance();
    // Alice is unavailable on day 3.
    data.unavailabilities.insert(AssignmentData::unavail_key(0, 3));

    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution sol(data, evaluator);

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

TEST_CASE("AssignmentSolution: preference costs", "[assignment]") {
    auto data = make_small_instance();
    // Alice prefers Day shift on Monday (day 0), weight = 5.
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };

    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution sol(data, evaluator);

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

TEST_CASE("AssignmentSolution: swap employees", "[assignment]") {
    auto data = make_small_instance();
    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution sol(data, evaluator);

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

TEST_CASE("AssignmentSolution: recompute_cost matches incremental", "[assignment]") {
    auto data = make_small_instance();
    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution sol(data, evaluator);

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

TEST_CASE("AssignmentSolution: skill-based demand", "[assignment]") {
    auto data = make_small_instance();
    // Override day 0 day-shift demand to require "senior" skill.
    data.demand[AssignmentData::demand_key(0, 0)] = {
        .min_employees = 1, .max_employees = 2, .required_skill = "senior"};

    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution sol(data, evaluator);

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
