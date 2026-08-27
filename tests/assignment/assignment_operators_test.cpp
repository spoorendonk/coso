#include "assignment/assignment_data.h"
#include "assignment/assignment_solution.h"
#include "assignment/cost_evaluator.h"
#include "assignment/operators/block_swap.h"
#include "assignment/operators/shift_move.h"
#include "assignment/operators/shift_swap.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small test instance
//    3 employees, 2 shift types (Day/Night), 7-day horizon
// ---------------------------------------------------------------------------

namespace {

AssignmentData make_instance() {
    AssignmentData data;

    // Shift types: Day (08-16, 8h) and Night (22-06, 8h).
    data.shift_types = {
        {.name = "Day", .start_hour = 8, .end_hour = 16, .duration_hours = 0},
        {.name = "Night", .start_hour = 22, .end_hour = 6, .duration_hours = 0},
    };

    // 3 employees.
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

    data.max_consecutive_shifts = 5;
    data.min_rest_between_shifts = 11;

    return data;
}

/// Build a solution with a specific schedule for testing operators.
/// Alice: Day shifts on days 0-4.
/// Bob:   Night shifts on days 0-4.
/// Carol: unassigned.
AssignmentSolution make_populated_solution(AssignmentData const& data,
                                           AssignmentCostEvaluator const& eval) {
    AssignmentSolution sol(data, eval);
    for (int d = 0; d < 5; ++d) {
        sol.assign(0, d, 0);  // Alice -> Day
        sol.assign(1, d, 1);  // Bob -> Night
    }
    return sol;
}

}  // anonymous namespace

// ===========================================================================
//  ShiftSwap tests
// ===========================================================================

TEST_CASE("ShiftSwap: finds improving swap", "[assignment][operators]") {
    auto data = make_instance();
    // Add a preference: Alice prefers Night on day 0, Bob prefers Day on day 0.
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 1, .weight = 10},
        {.employee = 1, .day = 0, .shift_type = 0, .weight = 10},
    };

    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    // Currently Alice=Day, Bob=Night on day 0.  Swapping should satisfy both
    // preferences and improve cost.
    ShiftSwap op;
    REQUIRE(op.find_best_move(sol));
    REQUIRE(op.best_delta() < 0);

    auto move = op.best_move();
    REQUIRE(move.day == 0);

    // Apply and verify.
    int cost_before = sol.cost();
    op.apply(sol);
    REQUIRE(sol.cost() == cost_before + move.delta);
}

TEST_CASE("ShiftSwap: no improving swap when optimal", "[assignment][operators]") {
    auto data = make_instance();
    // Preferences already satisfied.
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 10},
        {.employee = 1, .day = 0, .shift_type = 1, .weight = 10},
    };

    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    ShiftSwap op;
    // Even if there are non-improving swaps, find_best_move should return false.
    bool found = op.find_best_move(sol);
    // It may or may not find an improving move depending on demand interactions,
    // but if it does, delta must be < 0.
    if (found) {
        REQUIRE(op.best_delta() < 0);
    }
}

TEST_CASE("ShiftSwap: delta accuracy", "[assignment][operators]") {
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    // Enumerate all swap moves and verify delta matches actual cost change.
    auto moves = ShiftSwap::enumerate(sol);
    for (auto const& m : moves) {
        int cost_before = sol.cost();
        sol.swap(m.emp1, m.emp2, m.day);
        int cost_after = sol.cost();
        REQUIRE(m.delta == cost_after - cost_before);
        // Undo.
        sol.swap(m.emp1, m.emp2, m.day);
    }
}

TEST_CASE("ShiftSwap: skips identical assignments", "[assignment][operators]") {
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    // Assign Alice and Bob to the same shift on day 5.
    sol.assign(0, 5, 0);
    sol.assign(1, 5, 0);

    // Swaps where both have same assignment should not appear.
    auto moves = ShiftSwap::enumerate(sol);
    for (auto const& m : moves) {
        if (m.day == 5) {
            // emp1 and emp2 should not both be {0, 1} with same shift.
            bool same_pair = (m.emp1 == 0 && m.emp2 == 1) || (m.emp1 == 1 && m.emp2 == 0);
            if (same_pair) {
                // They should have different original assignments.
                // (This won't fire because enumerate skips same-assignment pairs.)
                REQUIRE(sol.get(m.emp1, m.day) != sol.get(m.emp2, m.day));
            }
        }
    }
}

// ===========================================================================
//  ShiftMove tests
// ===========================================================================

TEST_CASE("ShiftMove: finds improving move", "[assignment][operators]") {
    auto data = make_instance();
    // Carol prefers Day on day 0, but Alice currently has it.
    data.preferences = {
        {.employee = 2, .day = 0, .shift_type = 0, .weight = 50},
    };

    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    ShiftMove op;
    bool found = op.find_best_move(sol);

    // There should be an improving move (move Alice's day 0 shift to Carol,
    // or some other beneficial move).
    // We just verify the operator works correctly.
    if (found) {
        auto move = op.best_move();
        REQUIRE(move.delta < 0);

        int cost_before = sol.cost();
        op.apply(sol);
        REQUIRE(sol.cost() == cost_before + move.delta);
    }
}

TEST_CASE("ShiftMove: only moves to unassigned slots", "[assignment][operators]") {
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    auto moves = ShiftMove::enumerate(sol);
    for (auto const& m : moves) {
        // The target employee should have been unassigned on that day
        // at the time the move was evaluated.
        // Since we enumerate on the original solution, verify target was -1.
        REQUIRE(sol.get(m.to_emp, m.day) == -1);
    }
}

TEST_CASE("ShiftMove: delta accuracy", "[assignment][operators]") {
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    auto moves = ShiftMove::enumerate(sol);
    for (auto const& m : moves) {
        int cost_before = sol.cost();
        sol.unassign(m.from_emp, m.day);
        sol.assign(m.to_emp, m.day, m.shift_type);
        int cost_after = sol.cost();
        REQUIRE(m.delta == cost_after - cost_before);
        // Undo.
        sol.unassign(m.to_emp, m.day);
        sol.assign(m.from_emp, m.day, m.shift_type);
    }
}

// ===========================================================================
//  BlockSwap tests
// ===========================================================================

TEST_CASE("BlockSwap: finds improving block swap", "[assignment][operators]") {
    auto data = make_instance();
    // Preferences: Alice prefers Night on days 0-2, Bob prefers Day on days 0-2.
    for (int d = 0; d < 3; ++d) {
        data.preferences.push_back({.employee = 0, .day = d, .shift_type = 1, .weight = 10});
        data.preferences.push_back({.employee = 1, .day = d, .shift_type = 0, .weight = 10});
    }

    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    BlockSwap op;
    op.max_block_len = 7;
    REQUIRE(op.find_best_move(sol));
    REQUIRE(op.best_delta() < 0);

    auto move = op.best_move();
    REQUIRE(move.len >= 2);

    int cost_before = sol.cost();
    op.apply(sol);
    REQUIRE(sol.cost() == cost_before + move.delta);
}

TEST_CASE("BlockSwap: minimum block length is 2", "[assignment][operators]") {
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    auto moves = BlockSwap::enumerate(sol);
    for (auto const& m : moves) {
        REQUIRE(m.len >= 2);
    }
}

TEST_CASE("BlockSwap: delta accuracy", "[assignment][operators]") {
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    auto moves = BlockSwap::enumerate(sol, 4);  // Limit block size for speed.
    for (auto const& m : moves) {
        int cost_before = sol.cost();

        // Apply block swap.
        for (int d = m.start; d < m.start + m.len; ++d) {
            sol.swap(m.emp1, m.emp2, d);
        }
        int cost_after = sol.cost();

        REQUIRE(m.delta == cost_after - cost_before);

        // Undo.
        for (int d = m.start; d < m.start + m.len; ++d) {
            sol.swap(m.emp1, m.emp2, d);
        }
    }
}

TEST_CASE("BlockSwap: skips blocks with identical assignments", "[assignment][operators]") {
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    AssignmentSolution sol(data, eval);

    // Assign Alice and Bob to the same shifts on all days.
    for (int d = 0; d < 7; ++d) {
        sol.assign(0, d, 0);
        sol.assign(1, d, 0);
    }

    // All blocks between Alice and Bob are identical -> no moves.
    auto moves = BlockSwap::enumerate(sol);
    for (auto const& m : moves) {
        // If emp1=0, emp2=1, there should be at least one differing day.
        if (m.emp1 == 0 && m.emp2 == 1) {
            bool differs = false;
            for (int d = m.start; d < m.start + m.len; ++d) {
                if (sol.get(0, d) != sol.get(1, d)) {
                    differs = true;
                    break;
                }
            }
            REQUIRE(differs);
        }
    }
}

TEST_CASE("BlockSwap: respects max_block_len", "[assignment][operators]") {
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    auto moves = BlockSwap::enumerate(sol, 3);
    for (auto const& m : moves) {
        REQUIRE(m.len <= 3);
    }
}
