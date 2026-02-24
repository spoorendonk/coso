#include "assignment/assignment_data.h"
#include "assignment/assignment_solution.h"
#include "assignment/cost_evaluator.h"
#include "assignment/operators/pillar_move.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small test instance
//    4 employees, 2 shift types (Day/Night), 7-day horizon
// ---------------------------------------------------------------------------

namespace {

AssignmentData make_instance()
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

    data.horizon = 7;

    // Demand: 1 nurse per day shift, 1 per night shift, every day.
    for (int d = 0; d < 7; ++d) {
        data.demand[AssignmentData::demand_key(0, d)] = {
            .min_employees = 1, .max_employees = 2, .required_skill = ""};
        data.demand[AssignmentData::demand_key(1, d)] = {
            .min_employees = 1, .max_employees = 1, .required_skill = ""};
    }

    data.max_consecutive_shifts  = 5;
    data.min_rest_between_shifts = 11;

    return data;
}

/// Build a solution:
/// Alice: Day shifts on days 0-4.
/// Bob:   Night shifts on days 0-4.
/// Carol: unassigned.
/// Dave:  unassigned.
AssignmentSolution make_populated_solution(
    AssignmentData const& data,
    AssignmentCostEvaluator const& eval)
{
    AssignmentSolution sol(data, eval);
    for (int d = 0; d < 5; ++d) {
        sol.assign(0, d, 0);  // Alice -> Day
        sol.assign(1, d, 1);  // Bob -> Night
    }
    return sol;
}

} // anonymous namespace

// ===========================================================================
//  PillarMove tests
// ===========================================================================

TEST_CASE("PillarMove: finds improving move", "[assignment][pillar]")
{
    auto data = make_instance();
    // Carol strongly prefers Day on days 0-2.
    for (int d = 0; d < 3; ++d) {
        data.preferences.push_back(
            {.employee = 2, .day = d, .shift_type = 0, .weight = 50});
    }

    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    PillarMove op;
    op.max_block_len = 7;
    bool found = op.find_best_move(sol);

    // There should be an improving move (move Alice's block to Carol).
    if (found) {
        REQUIRE(op.best_delta() < 0);

        auto move = op.best_move();
        int cost_before = sol.cost();
        op.apply(sol);
        REQUIRE(sol.cost() == cost_before + move.delta);
    }
}

TEST_CASE("PillarMove: target must be unassigned", "[assignment][pillar]")
{
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    auto moves = PillarMove::enumerate(sol);
    for (auto const& m : moves) {
        // All days in the block must be unassigned for the target.
        for (int d = m.start; d < m.start + m.len; ++d) {
            REQUIRE(sol.get(m.to_emp, d) == -1);
        }
    }
}

TEST_CASE("PillarMove: delta accuracy", "[assignment][pillar]")
{
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    auto moves = PillarMove::enumerate(sol, 4);
    for (auto const& m : moves) {
        int cost_before = sol.cost();

        // Apply pillar move.
        std::vector<int> shift_types;
        for (int d = m.start; d < m.start + m.len; ++d) {
            shift_types.push_back(sol.get(m.from_emp, d));
            sol.unassign(m.from_emp, d);
            sol.assign(m.to_emp, d, shift_types.back());
        }
        int cost_after = sol.cost();
        REQUIRE(m.delta == cost_after - cost_before);

        // Undo.
        for (int i = m.len - 1; i >= 0; --i) {
            int d = m.start + i;
            sol.unassign(m.to_emp, d);
            sol.assign(m.from_emp, d, shift_types[i]);
        }
    }
}

TEST_CASE("PillarMove: source must be assigned", "[assignment][pillar]")
{
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    auto moves = PillarMove::enumerate(sol);
    for (auto const& m : moves) {
        for (int d = m.start; d < m.start + m.len; ++d) {
            REQUIRE(sol.get(m.from_emp, d) >= 0);
        }
    }
}

// ===========================================================================
//  PillarSwap tests
// ===========================================================================

TEST_CASE("PillarSwap: finds improving swap", "[assignment][pillar]")
{
    auto data = make_instance();
    // Alice prefers Night on days 0-2, Bob prefers Day on days 0-2.
    for (int d = 0; d < 3; ++d) {
        data.preferences.push_back(
            {.employee = 0, .day = d, .shift_type = 1, .weight = 10});
        data.preferences.push_back(
            {.employee = 1, .day = d, .shift_type = 0, .weight = 10});
    }

    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    PillarSwap op;
    op.max_block_len = 7;
    REQUIRE(op.find_best_move(sol));
    REQUIRE(op.best_delta() < 0);

    auto move = op.best_move();
    int cost_before = sol.cost();
    op.apply(sol);
    REQUIRE(sol.cost() == cost_before + move.delta);
}

TEST_CASE("PillarSwap: both employees must be assigned", "[assignment][pillar]")
{
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    auto moves = PillarSwap::enumerate(sol);
    for (auto const& m : moves) {
        for (int d = m.start; d < m.start + m.len; ++d) {
            REQUIRE(sol.get(m.emp1, d) >= 0);
            REQUIRE(sol.get(m.emp2, d) >= 0);
        }
    }
}

TEST_CASE("PillarSwap: skips identical blocks", "[assignment][pillar]")
{
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    AssignmentSolution sol(data, eval);

    // Assign Alice and Bob the same shifts.
    for (int d = 0; d < 5; ++d) {
        sol.assign(0, d, 0);
        sol.assign(1, d, 0);
    }

    auto moves = PillarSwap::enumerate(sol);
    for (auto const& m : moves) {
        if (m.emp1 == 0 && m.emp2 == 1) {
            // At least one day in the block must differ.
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

TEST_CASE("PillarSwap: delta accuracy", "[assignment][pillar]")
{
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    auto moves = PillarSwap::enumerate(sol, 4);
    for (auto const& m : moves) {
        int cost_before = sol.cost();

        for (int d = m.start; d < m.start + m.len; ++d)
            sol.swap(m.emp1, m.emp2, d);
        int cost_after = sol.cost();
        REQUIRE(m.delta == cost_after - cost_before);

        // Undo.
        for (int d = m.start; d < m.start + m.len; ++d)
            sol.swap(m.emp1, m.emp2, d);
    }
}

// ===========================================================================
//  PillarRotate tests
// ===========================================================================

TEST_CASE("PillarRotate: finds improving rotation with 3 employees",
          "[assignment][pillar]")
{
    auto data = make_instance();
    // Set up preferences that favor a cyclic rotation.
    // Alice (0) wants Night, Bob (1) wants Day, Carol (2) wants what Bob had.
    for (int d = 0; d < 3; ++d) {
        data.preferences.push_back(
            {.employee = 0, .day = d, .shift_type = 1, .weight = 20});
        data.preferences.push_back(
            {.employee = 2, .day = d, .shift_type = 0, .weight = 20});
    }

    AssignmentCostEvaluator eval(data);
    AssignmentSolution sol(data, eval);

    // Alice=Day, Bob=Night, Carol=Day on days 0-2.
    for (int d = 0; d < 3; ++d) {
        sol.assign(0, d, 0);  // Alice -> Day
        sol.assign(1, d, 1);  // Bob -> Night
        sol.assign(2, d, 0);  // Carol -> Day
    }

    PillarRotate op;
    op.max_block_len = 3;
    bool found = op.find_best_move(sol);

    if (found) {
        REQUIRE(op.best_delta() < 0);

        auto move = op.best_move();
        int cost_before = sol.cost();
        op.apply(sol);
        REQUIRE(sol.cost() == cost_before + move.delta);
    }
}

TEST_CASE("PillarRotate: skips trivial rotations", "[assignment][pillar]")
{
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    AssignmentSolution sol(data, eval);

    // All three employees have the same shift on all days.
    for (int d = 0; d < 5; ++d) {
        sol.assign(0, d, 0);
        sol.assign(1, d, 0);
        sol.assign(2, d, 0);
    }

    auto moves = PillarRotate::enumerate(sol);
    for (auto const& m : moves) {
        // Every enumerated move must have at least one differing day.
        bool nontrivial = false;
        for (int d = m.start; d < m.start + m.len; ++d) {
            int s0 = sol.get(m.emp0, d);
            int s1 = sol.get(m.emp1, d);
            int s2 = sol.get(m.emp2, d);
            if (s0 != s1 || s1 != s2) {
                nontrivial = true;
                break;
            }
        }
        REQUIRE(nontrivial);
    }
}

TEST_CASE("PillarRotate: delta accuracy", "[assignment][pillar]")
{
    auto data = make_instance();
    AssignmentCostEvaluator eval(data);
    AssignmentSolution sol(data, eval);

    // Set up a schedule with different shifts.
    for (int d = 0; d < 4; ++d) {
        sol.assign(0, d, 0);  // Alice -> Day
        sol.assign(1, d, 1);  // Bob -> Night
        sol.assign(2, d, 0);  // Carol -> Day
    }

    auto moves = PillarRotate::enumerate(sol, 2);
    for (auto const& m : moves) {
        int cost_before = sol.cost();

        // Apply rotation: e0<-e1, e1<-e2, e2<-e0.
        PillarRotate::apply_rotation(sol, m.emp0, m.emp1, m.emp2,
                                     m.start, m.len);
        int cost_after = sol.cost();
        REQUIRE(m.delta == cost_after - cost_before);

        // Undo: reverse rotation.
        PillarRotate::apply_rotation(sol, m.emp0, m.emp2, m.emp1,
                                     m.start, m.len);
    }
}

// ===========================================================================
//  VND integration test
// ===========================================================================

TEST_CASE("pillar_vnd: improves solution", "[assignment][pillar]")
{
    auto data = make_instance();
    // Preferences that favor reassignment.
    for (int d = 0; d < 3; ++d) {
        data.preferences.push_back(
            {.employee = 2, .day = d, .shift_type = 0, .weight = 50});
        data.preferences.push_back(
            {.employee = 0, .day = d, .shift_type = 1, .weight = 50});
    }

    AssignmentCostEvaluator eval(data);
    auto sol = make_populated_solution(data, eval);

    int cost_before = sol.cost();
    int delta = pillar_vnd(sol);

    // VND should improve or be neutral.
    REQUIRE(delta <= 0);
    REQUIRE(sol.cost() == cost_before + delta);

    // Verify cost consistency.
    sol.recompute_cost();
    REQUIRE(sol.cost() == cost_before + delta);
}
