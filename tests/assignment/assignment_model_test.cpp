#include <catch2/catch_test_macros.hpp>

#include "model/assignment_model.h"
#include "assignment/assignment_data.h"

using namespace coso;

// ===========================================================================
//  AssignmentData unit tests
// ===========================================================================

TEST_CASE("AssignmentData: shift type effective duration", "[assignment]")
{
    SECTION("explicit duration overrides start/end")
    {
        AssignmentData::ShiftType st{.name = "Custom", .start_hour = 8,
                                     .end_hour = 16, .duration_hours = 10};
        REQUIRE(st.effective_duration() == 10);
    }

    SECTION("computed from start/end (day shift)")
    {
        AssignmentData::ShiftType st{.name = "Day", .start_hour = 8,
                                     .end_hour = 16, .duration_hours = 0};
        REQUIRE(st.effective_duration() == 8);
    }

    SECTION("overnight shift wraps around 24h")
    {
        AssignmentData::ShiftType st{.name = "Night", .start_hour = 22,
                                     .end_hour = 6, .duration_hours = 0};
        REQUIRE(st.effective_duration() == 8);
    }
}

TEST_CASE("AssignmentData: demand key helpers", "[assignment]")
{
    auto k1 = AssignmentData::demand_key(0, 0);
    auto k2 = AssignmentData::demand_key(0, 1);
    auto k3 = AssignmentData::demand_key(1, 0);

    REQUIRE(k1 != k2);
    REQUIRE(k1 != k3);
    REQUIRE(k2 != k3);
}

TEST_CASE("AssignmentData: unavailability helpers", "[assignment]")
{
    AssignmentData data;
    data.unavailabilities.insert(AssignmentData::unavail_key(0, 2));

    REQUIRE(data.is_unavailable(0, 2));
    REQUIRE_FALSE(data.is_unavailable(0, 1));
    REQUIRE_FALSE(data.is_unavailable(1, 2));
}

// ===========================================================================
//  AssignmentModel API tests
// ===========================================================================

TEST_CASE("AssignmentModel: add shift types returns sequential ids",
          "[assignment]")
{
    AssignmentModel model;
    int morning = model.add_shift_type({.name = "Morning", .start_hour = 6,
                                        .end_hour = 14});
    int evening = model.add_shift_type({.name = "Evening", .start_hour = 14,
                                        .end_hour = 22});
    int night   = model.add_shift_type({.name = "Night", .start_hour = 22,
                                        .end_hour = 6});

    REQUIRE(morning == 0);
    REQUIRE(evening == 1);
    REQUIRE(night == 2);
}

TEST_CASE("AssignmentModel: add employees returns sequential ids",
          "[assignment]")
{
    AssignmentModel model;
    int alice = model.add_employee({.name = "Alice", .skills = {"nurse"}});
    int bob   = model.add_employee({.name = "Bob", .skills = {"nurse", "senior"}});

    REQUIRE(alice == 0);
    REQUIRE(bob == 1);
}

TEST_CASE("AssignmentModel: demand constraints are stored", "[assignment]")
{
    AssignmentModel model;
    model.add_shift_type({.name = "Day"});
    model.add_employee({.name = "Alice"});
    model.set_horizon(7);

    // Specific day demand.
    model.add_demand(0, 0, {.min_employees = 2, .max_employees = 5});

    // All-day demand.
    model.add_demand(0, {.min_employees = 1});

    // The model compiles internally on solve(); we just verify it doesn't crash
    // and returns a result.
    auto result = model.solve(TimeLimit(0.1));
    // Day 0 is intentionally over-constrained (needs 2, only 1 employee).
    REQUIRE_FALSE(result.unassigned().empty());
}

TEST_CASE("AssignmentModel: hard constraints", "[assignment]")
{
    AssignmentModel model;
    model.add_shift_type({.name = "Day"});
    model.add_employee({.name = "Alice"});
    model.set_horizon(14);

    model.set_max_consecutive_shifts(5);
    model.set_min_rest_between_shifts(11);
    model.add_forbidden_sequence({0, 0, 0});  // No triple day shifts.

    auto result = model.solve(TimeLimit(0.1));
    REQUIRE(result.feasible());
    REQUIRE(result.elapsed_seconds() >= 0.0);
}

TEST_CASE("AssignmentModel: preferences and unavailability", "[assignment]")
{
    AssignmentModel model;
    int day = model.add_shift_type({.name = "Day"});
    int alice = model.add_employee({.name = "Alice"});
    int bob   = model.add_employee({.name = "Bob"});
    model.set_horizon(7);

    // Alice prefers day shift on Monday (day 0).
    model.add_preference(alice, 0, day, 10);

    // Bob is unavailable on Wednesday (day 2).
    model.add_unavailability(bob, 2);

    auto result = model.solve(TimeLimit(0.1));
    REQUIRE(result.feasible());
}

TEST_CASE("AssignmentModel: replanning with published schedule", "[assignment]")
{
    AssignmentModel model;
    model.add_shift_type({.name = "Day"});
    model.add_employee({.name = "Alice"});
    model.add_employee({.name = "Bob"});
    model.set_horizon(7);

    // Published schedule: employee 0 works shift 0 every day, employee 1 off.
    std::vector<std::vector<int>> schedule = {
        {0, 0, 0, 0, 0, -1, -1},  // Alice
        {-1, -1, -1, -1, -1, 0, 0},  // Bob
    };
    model.set_published_schedule(schedule);
    model.set_change_penalty(100);

    auto result = model.solve(TimeLimit(0.1));
    REQUIRE(result.elapsed_seconds() >= 0.0);
}

TEST_CASE("AssignmentModel: solve returns result with elapsed time",
          "[assignment]")
{
    AssignmentModel model;
    model.add_shift_type({.name = "Morning"});
    model.add_employee({.name = "Nurse A"});
    model.set_horizon(7);
    model.add_demand(0, {.min_employees = 1});

    auto result = model.solve(TimeLimit(0.5));

    // But elapsed time should be non-negative.
    REQUIRE(result.elapsed_seconds() >= 0.0);
    REQUIRE(result.cost() >= 0.0);
}

TEST_CASE("AssignmentModel: feasible baseline instance", "[assignment]")
{
    AssignmentModel model;
    model.add_shift_type({.name = "Day"});
    model.add_employee({.name = "Alice"});
    model.add_employee({.name = "Bob"});
    model.set_horizon(4);
    model.add_demand(0, {.min_employees = 1, .max_employees = 1});

    auto result = model.solve(TimeLimit(0.5));

    REQUIRE(result.feasible());
    REQUIRE(result.assignments().size() == 4);
    REQUIRE(result.unassigned().empty());
}

TEST_CASE("AssignmentModel: empty model returns infeasible", "[assignment]")
{
    AssignmentModel model;
    auto result = model.solve(TimeLimit(0.1));
    REQUIRE_FALSE(result.feasible());
}

TEST_CASE("AssignmentModel: missing horizon returns infeasible", "[assignment]")
{
    AssignmentModel model;
    model.add_shift_type({.name = "Day"});
    model.add_employee({.name = "Alice"});
    // No set_horizon call.

    auto result = model.solve(TimeLimit(0.1));
    REQUIRE_FALSE(result.feasible());
}
