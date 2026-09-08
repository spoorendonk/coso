#include "model/rostering_model.h"

#include "rostering/rostering_data.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ===========================================================================
//  RosteringData unit tests
// ===========================================================================

TEST_CASE("RosteringData: shift type effective duration", "[rostering]") {
    SECTION("explicit duration overrides start/end") {
        RosteringData::ShiftType st{
            .name = "Custom", .start_hour = 8, .end_hour = 16, .duration_hours = 10};
        REQUIRE(st.effective_duration() == 10);
    }

    SECTION("computed from start/end (day shift)") {
        RosteringData::ShiftType st{
            .name = "Day", .start_hour = 8, .end_hour = 16, .duration_hours = 0};
        REQUIRE(st.effective_duration() == 8);
    }

    SECTION("overnight shift wraps around 24h") {
        RosteringData::ShiftType st{
            .name = "Night", .start_hour = 22, .end_hour = 6, .duration_hours = 0};
        REQUIRE(st.effective_duration() == 8);
    }
}

TEST_CASE("RosteringData: demand key helpers", "[rostering]") {
    auto k1 = RosteringData::demand_key(0, 0);
    auto k2 = RosteringData::demand_key(0, 1);
    auto k3 = RosteringData::demand_key(1, 0);

    REQUIRE(k1 != k2);
    REQUIRE(k1 != k3);
    REQUIRE(k2 != k3);
}

TEST_CASE("RosteringData: unavailability helpers", "[rostering]") {
    RosteringData data;
    data.unavailabilities.insert(RosteringData::unavail_key(0, 2));

    REQUIRE(data.is_unavailable(0, 2));
    REQUIRE_FALSE(data.is_unavailable(0, 1));
    REQUIRE_FALSE(data.is_unavailable(1, 2));
}

// ===========================================================================
//  RosteringModel API tests
// ===========================================================================

TEST_CASE("RosteringModel: add shift types returns sequential ids", "[rostering]") {
    RosteringModel model;
    int morning = model.add_shift_type({.name = "Morning", .start_hour = 6, .end_hour = 14});
    int evening = model.add_shift_type({.name = "Evening", .start_hour = 14, .end_hour = 22});
    int night = model.add_shift_type({.name = "Night", .start_hour = 22, .end_hour = 6});

    REQUIRE(morning == 0);
    REQUIRE(evening == 1);
    REQUIRE(night == 2);
}

TEST_CASE("RosteringModel: add employees returns sequential ids", "[rostering]") {
    RosteringModel model;
    int alice = model.add_employee({.name = "Alice", .skills = {"nurse"}});
    int bob = model.add_employee({.name = "Bob", .skills = {"nurse", "senior"}});

    REQUIRE(alice == 0);
    REQUIRE(bob == 1);
}

TEST_CASE("RosteringModel: demand constraints are stored", "[rostering]") {
    RosteringModel model;
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

TEST_CASE("RosteringModel: hard constraints", "[rostering]") {
    RosteringModel model;
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

TEST_CASE("RosteringModel: preferences and unavailability", "[rostering]") {
    RosteringModel model;
    int day = model.add_shift_type({.name = "Day"});
    int alice = model.add_employee({.name = "Alice"});
    int bob = model.add_employee({.name = "Bob"});
    model.set_horizon(7);

    // Alice prefers day shift on Monday (day 0).
    model.add_preference(alice, 0, day, 10);

    // Bob is unavailable on Wednesday (day 2).
    model.add_unavailability(bob, 2);

    auto result = model.solve(TimeLimit(0.1));
    REQUIRE(result.feasible());
}

TEST_CASE("RosteringModel: replanning with published schedule", "[rostering]") {
    RosteringModel model;
    model.add_shift_type({.name = "Day"});
    model.add_employee({.name = "Alice"});
    model.add_employee({.name = "Bob"});
    model.set_horizon(7);

    // Published schedule: employee 0 works shift 0 every day, employee 1 off.
    std::vector<std::vector<int>> schedule = {
        {0, 0, 0, 0, 0, -1, -1},     // Alice
        {-1, -1, -1, -1, -1, 0, 0},  // Bob
    };
    model.set_published_schedule(schedule);
    model.set_change_penalty(100);

    auto result = model.solve(TimeLimit(0.1));
    REQUIRE(result.elapsed_seconds() >= 0.0);
}

TEST_CASE("RosteringModel: solve returns result with elapsed time", "[rostering]") {
    RosteringModel model;
    model.add_shift_type({.name = "Morning"});
    model.add_employee({.name = "Nurse A"});
    model.set_horizon(7);
    model.add_demand(0, {.min_employees = 1});

    auto result = model.solve(TimeLimit(0.5));

    // But elapsed time should be non-negative.
    REQUIRE(result.elapsed_seconds() >= 0.0);
    REQUIRE(result.cost() >= 0.0);
}

TEST_CASE("RosteringModel: feasible baseline instance", "[rostering]") {
    RosteringModel model;
    model.add_shift_type({.name = "Day"});
    model.add_employee({.name = "Alice"});
    model.add_employee({.name = "Bob"});
    model.set_horizon(4);
    model.add_demand(0, {.min_employees = 1, .max_employees = 1});

    auto result = model.solve(TimeLimit(0.5));

    REQUIRE(result.feasible());
    REQUIRE(result.roster().size() == 4);
    REQUIRE(result.unassigned().empty());
}

TEST_CASE("RosteringModel: empty model returns infeasible", "[rostering]") {
    RosteringModel model;
    auto result = model.solve(TimeLimit(0.1));
    REQUIRE_FALSE(result.feasible());
}

TEST_CASE("RosteringModel: missing horizon returns infeasible", "[rostering]") {
    RosteringModel model;
    model.add_shift_type({.name = "Day"});
    model.add_employee({.name = "Alice"});
    // No set_horizon call.

    auto result = model.solve(TimeLimit(0.1));
    REQUIRE_FALSE(result.feasible());
}

TEST_CASE("RosteringModel: a copy carries the declaration", "[rostering]") {
    // Regression for #197: the model kept its state in a file-local map keyed
    // on `this`, so a copy shared none of it and solved as an empty model --
    // silently, with feasible() == false and no assignments.
    RosteringModel a;
    a.set_horizon(3);
    int day = a.add_shift_type({.name = "Day", .start_hour = 8, .end_hour = 16});
    a.add_employee({.name = "Alice"});
    a.add_demand(day, DemandParams{.min_employees = 1});

    RosteringModel b = a;

    auto ra = a.solve(TimeLimit(0.1));
    auto rb = b.solve(TimeLimit(0.1));

    REQUIRE(ra.feasible());
    REQUIRE(rb.feasible() == ra.feasible());
    REQUIRE(rb.cost() == ra.cost());
    REQUIRE(rb.roster().size() == ra.roster().size());
}
