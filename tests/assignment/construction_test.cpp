#include "assignment/construction.h"

#include "assignment/assignment_data.h"
#include "assignment/assignment_solution.h"
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
//  FFD construction
// ---------------------------------------------------------------------------

TEST_CASE("construct_ffd: small instance meets demand", "[assignment][construction]") {
    auto data = make_small_instance();
    AssignmentCostEvaluator evaluator(data);

    auto sol = construct_ffd(data, evaluator);

    // All demand should be satisfied: 0 demand cost.
    REQUIRE(sol.demand_cost() == 0);
}

TEST_CASE("construct_ffd: solution is feasible", "[assignment][construction]") {
    auto data = make_small_instance();
    AssignmentCostEvaluator evaluator(data);

    auto sol = construct_ffd(data, evaluator);

    // No hard constraint violations.
    REQUIRE(sol.is_feasible());
}

TEST_CASE("construct_ffd: respects unavailability", "[assignment][construction]") {
    auto data = make_small_instance();
    // Alice unavailable on days 0-2.
    for (int d = 0; d < 3; ++d) {
        data.unavailabilities.insert(AssignmentData::unavail_key(0, d));
    }

    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_ffd(data, evaluator);

    // Alice should not be assigned on days 0-2.
    for (int d = 0; d < 3; ++d) {
        REQUIRE(sol.get(0, d) == -1);
    }

    // Solution should still be feasible.
    REQUIRE(sol.is_feasible());
}

TEST_CASE("construct_ffd: respects consecutive day limit", "[assignment][construction]") {
    auto data = make_small_instance();
    // Tighten consecutive days to 3 for all employees.
    data.max_consecutive_shifts = 3;
    for (auto& emp : data.employees) {
        emp.max_consecutive_days = 3;
    }

    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_ffd(data, evaluator);

    // Check no employee works more than 3 consecutive days.
    for (int e = 0; e < data.num_employees(); ++e) {
        int run = 0;
        for (int d = 0; d < data.horizon; ++d) {
            if (sol.get(e, d) >= 0) {
                ++run;
            } else {
                run = 0;
            }
            REQUIRE(run <= 3);
        }
    }
}

TEST_CASE("construct_ffd: respects forbidden sequences", "[assignment][construction]") {
    auto data = make_small_instance();
    // Forbid Night -> Day sequence.
    data.forbidden_sequences = {{1, 0}};

    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_ffd(data, evaluator);

    // Check no employee has Night followed by Day.
    for (int e = 0; e < data.num_employees(); ++e) {
        for (int d = 0; d + 1 < data.horizon; ++d) {
            if (sol.get(e, d) == 1) {
                REQUIRE(sol.get(e, d + 1) != 0);
            }
        }
    }
}

TEST_CASE("construct_ffd: skill-based demand", "[assignment][construction]") {
    auto data = make_small_instance();
    // Require "senior" skill for day shift on days 0-4 only (Carol can cover 5).
    for (int d = 0; d < 5; ++d) {
        data.demand[AssignmentData::demand_key(0, d)] = {
            .min_employees = 1, .max_employees = 2, .required_skill = "senior"};
    }
    // Days 5-6: no skill requirement (any employee).
    for (int d = 5; d < 7; ++d) {
        data.demand[AssignmentData::demand_key(0, d)] = {
            .min_employees = 1, .max_employees = 2, .required_skill = ""};
    }

    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_ffd(data, evaluator);

    // Carol (the only "senior") should be assigned to day shifts on days 0-4.
    for (int d = 0; d < 5; ++d) {
        REQUIRE(sol.get(2, d) == 0);  // Carol = employee 2, Day = shift 0
    }

    // All demand should be met.
    REQUIRE(sol.demand_cost() == 0);
}

// ---------------------------------------------------------------------------
//  Greedy construction
// ---------------------------------------------------------------------------

TEST_CASE("construct_greedy: small instance meets demand", "[assignment][construction]") {
    auto data = make_small_instance();
    AssignmentCostEvaluator evaluator(data);

    auto sol = construct_greedy(data, evaluator);

    REQUIRE(sol.demand_cost() == 0);
}

TEST_CASE("construct_greedy: solution is feasible", "[assignment][construction]") {
    auto data = make_small_instance();
    AssignmentCostEvaluator evaluator(data);

    auto sol = construct_greedy(data, evaluator);

    REQUIRE(sol.is_feasible());
}

TEST_CASE("construct_greedy: respects unavailability", "[assignment][construction]") {
    auto data = make_small_instance();
    for (int d = 0; d < 3; ++d) {
        data.unavailabilities.insert(AssignmentData::unavail_key(0, d));
    }

    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_greedy(data, evaluator);

    for (int d = 0; d < 3; ++d) {
        REQUIRE(sol.get(0, d) == -1);
    }

    REQUIRE(sol.is_feasible());
}

TEST_CASE("construct_greedy: cost is better than empty solution", "[assignment][construction]") {
    auto data = make_small_instance();
    AssignmentCostEvaluator evaluator(data);

    AssignmentSolution empty(data, evaluator);
    auto sol = construct_greedy(data, evaluator);

    // Constructed solution should have lower cost than empty.
    REQUIRE(sol.cost() < empty.cost());
}

TEST_CASE("construct_ffd: cost is better than empty solution", "[assignment][construction]") {
    auto data = make_small_instance();
    AssignmentCostEvaluator evaluator(data);

    AssignmentSolution empty(data, evaluator);
    auto sol = construct_ffd(data, evaluator);

    REQUIRE(sol.cost() < empty.cost());
}
