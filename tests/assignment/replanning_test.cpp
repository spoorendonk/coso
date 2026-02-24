#include "assignment/replanning.h"
#include "assignment/assignment_data.h"
#include "assignment/assignment_solution.h"
#include "assignment/construction.h"
#include "assignment/cost_evaluator.h"

#include <catch2/catch_test_macros.hpp>
#include <climits>
#include <tuple>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small test instance
//    4 employees, 2 shift types (Day/Night), 14-day horizon
// ---------------------------------------------------------------------------

namespace {

AssignmentData make_replan_instance()
{
    AssignmentData data;

    // Shift types: Day (08-16, 8h) and Night (22-06, 8h).
    data.shift_types = {
        {.name = "Day",   .start_hour = 8,  .end_hour = 16, .duration_hours = 0},
        {.name = "Night", .start_hour = 22, .end_hour = 6,  .duration_hours = 0},
    };

    // 4 employees.
    data.employees = {
        {.name = "Alice", .skills = {"nurse"}, .max_hours_per_week = 40,
         .max_consecutive_days = 5, .min_rest_hours = 11},
        {.name = "Bob",   .skills = {"nurse"}, .max_hours_per_week = 40,
         .max_consecutive_days = 5, .min_rest_hours = 11},
        {.name = "Carol", .skills = {"nurse", "senior"}, .max_hours_per_week = 40,
         .max_consecutive_days = 5, .min_rest_hours = 11},
        {.name = "Dave",  .skills = {"nurse"}, .max_hours_per_week = 40,
         .max_consecutive_days = 5, .min_rest_hours = 11},
    };

    data.horizon = 14;

    // Demand: 1 nurse per day shift, 1 per night shift, every day.
    for (int d = 0; d < 14; ++d) {
        data.demand[AssignmentData::demand_key(0, d)] = {
            .min_employees = 1, .max_employees = 2, .required_skill = ""};
        data.demand[AssignmentData::demand_key(1, d)] = {
            .min_employees = 1, .max_employees = 1, .required_skill = ""};
    }

    data.max_consecutive_shifts  = 5;
    data.min_rest_between_shifts = 11;

    return data;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
//  LockedCells tests
// ---------------------------------------------------------------------------

TEST_CASE("LockedCells: horizon_start locks early days",
          "[assignment][replanning]")
{
    auto data = make_replan_instance();
    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_greedy(data, evaluator);

    ReplanConfig config;
    config.horizon_start = 7;  // Lock first week.

    LockedCells locked(config, sol);

    // Days 0-6 should be locked for all employees.
    for (int e = 0; e < data.num_employees(); ++e) {
        for (int d = 0; d < 7; ++d) {
            REQUIRE(locked.is_locked(e, d));
        }
        // Days 7-13 should NOT be locked.
        for (int d = 7; d < 14; ++d) {
            REQUIRE_FALSE(locked.is_locked(e, d));
        }
    }
}

TEST_CASE("LockedCells: explicit locked assignments",
          "[assignment][replanning]")
{
    auto data = make_replan_instance();
    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_greedy(data, evaluator);

    ReplanConfig config;
    config.locked_assignments = {
        {0, 10, 0},  // Alice, day 10, Day shift
        {1, 12, 1},  // Bob, day 12, Night shift
    };

    LockedCells locked(config, sol);

    REQUIRE(locked.is_locked(0, 10));
    REQUIRE(locked.is_locked(1, 12));
    REQUIRE_FALSE(locked.is_locked(0, 11));
    REQUIRE_FALSE(locked.is_locked(2, 10));
}

// ---------------------------------------------------------------------------
//  Replanning tests
// ---------------------------------------------------------------------------

TEST_CASE("replan: locked shifts remain unchanged",
          "[assignment][replanning]")
{
    auto data = make_replan_instance();
    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_greedy(data, evaluator);

    // Record some assignments to lock.
    int alice_day3_shift = sol.get(0, 3);
    int bob_day5_shift   = sol.get(1, 5);

    // Force specific assignments if they are unassigned.
    if (alice_day3_shift < 0) {
        sol.assign(0, 3, 0);
        alice_day3_shift = 0;
    }
    if (bob_day5_shift < 0) {
        sol.assign(1, 5, 1);
        bob_day5_shift = 1;
    }

    ReplanConfig config;
    config.locked_assignments = {
        {0, 3, alice_day3_shift},
        {1, 5, bob_day5_shift},
    };

    replan(sol, config, data, evaluator);

    // Locked assignments must not change.
    REQUIRE(sol.get(0, 3) == alice_day3_shift);
    REQUIRE(sol.get(1, 5) == bob_day5_shift);
}

TEST_CASE("replan: new unavailabilities are respected",
          "[assignment][replanning]")
{
    auto data = make_replan_instance();
    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_greedy(data, evaluator);

    ReplanConfig config;
    // Make Carol unavailable on days 7-13.
    for (int d = 7; d < 14; ++d) {
        config.new_unavailabilities.push_back({2, d});
    }

    replan(sol, config, data, evaluator);

    // Carol should not be assigned on days 7-13.
    for (int d = 7; d < 14; ++d) {
        REQUIRE(sol.get(2, d) == -1);
    }
}

TEST_CASE("replan: partial replanning (only future days change)",
          "[assignment][replanning]")
{
    auto data = make_replan_instance();
    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_greedy(data, evaluator);

    // Record the first week's assignments.
    std::vector<std::vector<int>> first_week(data.num_employees());
    for (int e = 0; e < data.num_employees(); ++e) {
        for (int d = 0; d < 7; ++d) {
            first_week[e].push_back(sol.get(e, d));
        }
    }

    ReplanConfig config;
    config.horizon_start = 7;  // Lock first week.

    replan(sol, config, data, evaluator);

    // First week must be unchanged.
    for (int e = 0; e < data.num_employees(); ++e) {
        for (int d = 0; d < 7; ++d) {
            REQUIRE(sol.get(e, d) == first_week[e][d]);
        }
    }
}

TEST_CASE("replan: simple rostering instance end-to-end",
          "[assignment][replanning]")
{
    auto data = make_replan_instance();
    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_greedy(data, evaluator);

    int cost_before = sol.cost();

    // Lock the first 7 days, add a new preference for the second week.
    ReplanConfig config;
    config.horizon_start = 7;
    config.new_preferences = {
        {.employee = 0, .day = 8, .shift_type = 0, .weight = 100},
    };

    replan(sol, config, data, evaluator);

    // Solution should still have reasonable cost (not worse by a huge margin).
    // The locked first week contributes the same cost.
    REQUIRE(sol.cost() <= cost_before + 1000);
}

TEST_CASE("replan: locked assignments are set correctly even if different",
          "[assignment][replanning]")
{
    auto data = make_replan_instance();
    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_greedy(data, evaluator);

    // Force a specific locked assignment that differs from current.
    // First ensure employee 0 day 10 has shift 0 (Day).
    if (sol.get(0, 10) >= 0)
        sol.unassign(0, 10);
    sol.assign(0, 10, 0);

    // Now replan with it locked as Night shift (1).
    ReplanConfig config;
    config.locked_assignments = {
        {0, 10, 1},  // Lock to Night shift.
    };

    replan(sol, config, data, evaluator);

    // Must be set to the locked value.
    REQUIRE(sol.get(0, 10) == 1);
}

TEST_CASE("replan: demand is still met after replanning",
          "[assignment][replanning]")
{
    auto data = make_replan_instance();
    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_greedy(data, evaluator);

    REQUIRE(sol.demand_cost() == 0);  // Precondition: demand met.

    ReplanConfig config;
    config.horizon_start = 7;

    replan(sol, config, data, evaluator);

    // Demand for the first week (locked) is still met.
    // Second week should also be filled as much as possible.
    // Check that the demand cost is not worse than before replanning.
    // (With 4 employees and low demand, it should remain 0.)
    REQUIRE(sol.demand_cost() == 0);
}

TEST_CASE("replan: new preferences influence solution",
          "[assignment][replanning]")
{
    auto data = make_replan_instance();

    // Add a strong preference for Alice on day 10 for Day shift.
    ReplanConfig config;
    config.new_preferences = {
        {.employee = 0, .day = 10, .shift_type = 0, .weight = 500},
    };

    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_greedy(data, evaluator);

    replan(sol, config, data, evaluator);

    // With a strong preference, Alice should ideally get Day shift on day 10.
    // This is a soft test — it may not always hold if hard constraints prevent
    // it, but with 4 employees and low demand it should be achievable.
    REQUIRE(sol.get(0, 10) == 0);
}

TEST_CASE("replan: empty config produces valid solution",
          "[assignment][replanning]")
{
    auto data = make_replan_instance();
    AssignmentCostEvaluator evaluator(data);
    auto sol = construct_greedy(data, evaluator);

    ReplanConfig config;  // No locks, no new constraints.

    replan(sol, config, data, evaluator);

    // Should still produce a valid solution.
    REQUIRE(sol.demand_cost() == 0);
    REQUIRE(sol.is_feasible());
}
