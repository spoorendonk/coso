#include "assignment/constraints/constraint.h"

#include "assignment/assignment_data.h"

#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <memory>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small test instance
//    3 employees, 2 shift types (Day/Night), 7-day horizon
// ---------------------------------------------------------------------------

namespace {

AssignmentData make_instance() {
    AssignmentData data;

    data.shift_types = {
        {.name = "Day", .start_hour = 8, .end_hour = 16, .duration_hours = 0},
        {.name = "Night", .start_hour = 22, .end_hour = 6, .duration_hours = 0},
    };

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

/// Apply a move to a schedule in-place.
void apply_move(std::vector<std::vector<int>>& schedule, AssignmentMove const& move) {
    schedule[move.employee][move.day] = move.new_shift;
}

}  // anonymous namespace

// ===========================================================================
//  MaxConsecutiveConstraint
// ===========================================================================

TEST_CASE("MaxConsecutiveConstraint: no violation", "[assignment][constraint]") {
    auto data = make_instance();
    data.max_consecutive_shifts = 3;
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }

    MaxConsecutiveConstraint c(10000);

    // 3 consecutive days is fine.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 0;
    sched[0][1] = 0;
    sched[0][2] = 0;

    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("MaxConsecutiveConstraint: violation on 4th day", "[assignment][constraint]") {
    auto data = make_instance();
    data.max_consecutive_shifts = 3;
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }

    MaxConsecutiveConstraint c(10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 0;
    sched[0][1] = 0;
    sched[0][2] = 0;
    sched[0][3] = 0;  // 4th consecutive -> violation

    REQUIRE(c.evaluate(data, sched) == 10000);
}

TEST_CASE("MaxConsecutiveConstraint: delta matches full", "[assignment][constraint]") {
    auto data = make_instance();
    data.max_consecutive_shifts = 3;
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }

    MaxConsecutiveConstraint c(10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 0;
    sched[0][1] = 0;
    sched[0][2] = 0;

    // Assign 4th day: should create exactly 1 violation.
    AssignmentMove move{.employee = 0, .day = 3, .old_shift = -1, .new_shift = 0};
    int delta = c.evaluate_delta(data, sched, move);

    auto after = sched;
    apply_move(after, move);
    int expected = c.evaluate(data, after) - c.evaluate(data, sched);

    REQUIRE(delta == expected);
    REQUIRE(delta == 10000);
}

// ===========================================================================
//  MinRestConstraint
// ===========================================================================

TEST_CASE("MinRestConstraint: sufficient rest", "[assignment][constraint]") {
    auto data = make_instance();
    MinRestConstraint c(10000);

    // Day ends 16:00, Night starts 22:00 next day.
    // Rest = (24-16) + 22 = 30h -> OK.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 0;  // Day
    sched[0][1] = 1;  // Night

    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("MinRestConstraint: insufficient rest", "[assignment][constraint]") {
    AssignmentData data;
    data.shift_types = {
        {.name = "Late", .start_hour = 12, .end_hour = 20, .duration_hours = 0},
        {.name = "Early", .start_hour = 6, .end_hour = 14, .duration_hours = 0},
    };
    data.employees = {{.name = "X",
                       .skills = {},
                       .max_hours_per_week = 40,
                       .max_consecutive_days = 7,
                       .min_rest_hours = 11}};
    data.horizon = 2;
    data.min_rest_between_shifts = 11;

    MinRestConstraint c(10000);

    // Late ends 20:00, Early starts 06:00.
    // Rest = (24-20) + 6 = 10h < 11h -> violation.
    std::vector<std::vector<int>> sched(1, std::vector<int>(2, -1));
    sched[0][0] = 0;  // Late
    sched[0][1] = 1;  // Early

    REQUIRE(c.evaluate(data, sched) == 10000);
}

TEST_CASE("MinRestConstraint: delta matches full", "[assignment][constraint]") {
    AssignmentData data;
    data.shift_types = {
        {.name = "Late", .start_hour = 12, .end_hour = 20, .duration_hours = 0},
        {.name = "Early", .start_hour = 6, .end_hour = 14, .duration_hours = 0},
    };
    data.employees = {{.name = "X",
                       .skills = {},
                       .max_hours_per_week = 40,
                       .max_consecutive_days = 7,
                       .min_rest_hours = 11}};
    data.horizon = 3;
    data.min_rest_between_shifts = 11;

    MinRestConstraint c(10000);

    std::vector<std::vector<int>> sched(1, std::vector<int>(3, -1));
    sched[0][0] = 0;  // Late on day 0

    // Assign Early on day 1 -> violation with day 0.
    AssignmentMove move{.employee = 0, .day = 1, .old_shift = -1, .new_shift = 1};
    int delta = c.evaluate_delta(data, sched, move);

    auto after = sched;
    apply_move(after, move);
    int expected = c.evaluate(data, after) - c.evaluate(data, sched);

    REQUIRE(delta == expected);
    REQUIRE(delta == 10000);
}

// ===========================================================================
//  DemandConstraint
// ===========================================================================

TEST_CASE("DemandConstraint: empty schedule has understaffing", "[assignment][constraint]") {
    auto data = make_instance();
    DemandConstraint c(1000, 100);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    // 7 days * (1 understaffed day + 1 understaffed night) = 14 * 1000
    REQUIRE(c.evaluate(data, sched) == 14 * 1000);
}

TEST_CASE("DemandConstraint: satisfied demand", "[assignment][constraint]") {
    auto data = make_instance();
    DemandConstraint c(1000, 100);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    for (int d = 0; d < 7; ++d) {
        sched[0][d] = 0;  // Alice -> Day
        sched[1][d] = 1;  // Bob -> Night
    }

    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("DemandConstraint: overstaffing", "[assignment][constraint]") {
    auto data = make_instance();
    DemandConstraint c(1000, 100);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    // Fill all demands first.
    for (int d = 0; d < 7; ++d) {
        sched[0][d] = 0;  // Alice -> Day
        sched[1][d] = 1;  // Bob -> Night
    }
    // Carol also on Night day 0 -> night max is 1, so overstaffed by 1.
    sched[2][0] = 1;

    REQUIRE(c.evaluate(data, sched) == 100);  // 1 * overstaffing penalty
}

TEST_CASE("DemandConstraint: delta matches full", "[assignment][constraint]") {
    auto data = make_instance();
    DemandConstraint c(1000, 100);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    // Assign Alice to Day on day 0.
    AssignmentMove move{.employee = 0, .day = 0, .old_shift = -1, .new_shift = 0};
    int delta = c.evaluate_delta(data, sched, move);

    auto after = sched;
    apply_move(after, move);
    int expected = c.evaluate(data, after) - c.evaluate(data, sched);

    REQUIRE(delta == expected);
    // Was understaffed by 1 on day shift day 0; now satisfied -> -1000.
    REQUIRE(delta == -1000);
}

// ===========================================================================
//  ForbiddenSequenceConstraint
// ===========================================================================

TEST_CASE("ForbiddenSequenceConstraint: no forbidden sequences", "[assignment][constraint]") {
    auto data = make_instance();
    data.forbidden_sequences.clear();
    ForbiddenSequenceConstraint c(10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 1;
    sched[0][1] = 0;

    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("ForbiddenSequenceConstraint: Night->Day forbidden", "[assignment][constraint]") {
    auto data = make_instance();
    data.forbidden_sequences = {{1, 0}};  // Night -> Day
    ForbiddenSequenceConstraint c(10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 1;  // Night
    sched[0][1] = 0;  // Day

    REQUIRE(c.evaluate(data, sched) == 10000);

    // Day -> Night is OK.
    sched[0][0] = 0;
    sched[0][1] = 1;
    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("ForbiddenSequenceConstraint: delta matches full", "[assignment][constraint]") {
    auto data = make_instance();
    data.forbidden_sequences = {{1, 0}};  // Night -> Day
    ForbiddenSequenceConstraint c(10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 1;  // Night on day 0

    // Assign Day on day 1 -> creates forbidden Night->Day.
    AssignmentMove move{.employee = 0, .day = 1, .old_shift = -1, .new_shift = 0};
    int delta = c.evaluate_delta(data, sched, move);

    auto after = sched;
    apply_move(after, move);
    int expected = c.evaluate(data, after) - c.evaluate(data, sched);

    REQUIRE(delta == expected);
    REQUIRE(delta == 10000);
}

// ===========================================================================
//  PreferenceConstraint
// ===========================================================================

TEST_CASE("PreferenceConstraint: satisfied preference", "[assignment][constraint]") {
    auto data = make_instance();
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };
    PreferenceConstraint c(1);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 0;  // Alice gets preferred Day shift

    REQUIRE(c.evaluate(data, sched) == -5);
}

TEST_CASE("PreferenceConstraint: unsatisfied preference", "[assignment][constraint]") {
    auto data = make_instance();
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };
    PreferenceConstraint c(1);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 1;  // Night instead of preferred Day

    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("PreferenceConstraint: delta matches full", "[assignment][constraint]") {
    auto data = make_instance();
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };
    PreferenceConstraint c(1);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    // Assign preferred shift.
    AssignmentMove move{.employee = 0, .day = 0, .old_shift = -1, .new_shift = 0};
    int delta = c.evaluate_delta(data, sched, move);

    auto after = sched;
    apply_move(after, move);
    int expected = c.evaluate(data, after) - c.evaluate(data, sched);

    REQUIRE(delta == expected);
    REQUIRE(delta == -5);

    // Now remove the preferred shift.
    AssignmentMove move2{.employee = 0, .day = 0, .old_shift = 0, .new_shift = -1};
    int delta2 = c.evaluate_delta(data, after, move2);

    auto after2 = after;
    apply_move(after2, move2);
    int expected2 = c.evaluate(data, after2) - c.evaluate(data, after);

    REQUIRE(delta2 == expected2);
    REQUIRE(delta2 == 5);
}

// ===========================================================================
//  ConstraintEvaluator (composition)
// ===========================================================================

TEST_CASE("ConstraintEvaluator: composite evaluate", "[assignment][constraint]") {
    auto data = make_instance();
    data.max_consecutive_shifts = 3;
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }
    data.forbidden_sequences = {{1, 0}};
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };

    ConstraintEvaluator eval;
    eval.add(std::make_unique<MaxConsecutiveConstraint>(10000));
    eval.add(std::make_unique<MinRestConstraint>(10000));
    eval.add(std::make_unique<DemandConstraint>(1000, 100));
    eval.add(std::make_unique<ForbiddenSequenceConstraint>(10000));
    eval.add(std::make_unique<PreferenceConstraint>(1));

    REQUIRE(eval.size() == 5);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    // Empty schedule: demand violations only.
    int cost = eval.evaluate(data, sched);
    REQUIRE(cost == 14 * 1000);  // 14 understaffing violations

    // Assign Alice to preferred shift.
    sched[0][0] = 0;
    int cost2 = eval.evaluate(data, sched);
    // -1000 for satisfying day demand on day 0, -5 for preference.
    REQUIRE(cost2 == cost - 1000 - 5);
}

TEST_CASE("ConstraintEvaluator: composite delta matches full", "[assignment][constraint]") {
    auto data = make_instance();
    data.max_consecutive_shifts = 3;
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }
    data.forbidden_sequences = {{1, 0}};
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };

    ConstraintEvaluator eval;
    eval.add(std::make_unique<MaxConsecutiveConstraint>(10000));
    eval.add(std::make_unique<MinRestConstraint>(10000));
    eval.add(std::make_unique<DemandConstraint>(1000, 100));
    eval.add(std::make_unique<ForbiddenSequenceConstraint>(10000));
    eval.add(std::make_unique<PreferenceConstraint>(1));

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    AssignmentMove move{.employee = 0, .day = 0, .old_shift = -1, .new_shift = 0};
    int delta = eval.evaluate_delta(data, sched, move);

    auto after = sched;
    apply_move(after, move);
    int expected = eval.evaluate(data, after) - eval.evaluate(data, sched);

    REQUIRE(delta == expected);
}

TEST_CASE("ConstraintEvaluator: breakdown reports per-constraint costs",
          "[assignment][constraint]") {
    auto data = make_instance();
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };

    ConstraintEvaluator eval;
    eval.add(std::make_unique<DemandConstraint>(1000, 100));
    eval.add(std::make_unique<PreferenceConstraint>(1));

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 0;  // Preferred shift assigned

    auto bd = eval.breakdown(data, sched);
    REQUIRE(bd.size() == 2);
    REQUIRE(bd[0].first == "Demand");
    REQUIRE(bd[1].first == "Preference");
    REQUIRE(bd[1].second == -5);
}

// ===========================================================================
//  Delta accuracy: multiple consecutive moves
// ===========================================================================

TEST_CASE("Constraint delta: sequential moves stay accurate", "[assignment][constraint]") {
    auto data = make_instance();
    data.max_consecutive_shifts = 3;
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }
    data.forbidden_sequences = {{1, 0}};
    data.preferences = {
        {.employee = 0, .day = 2, .shift_type = 0, .weight = 10},
    };

    ConstraintEvaluator eval;
    eval.add(std::make_unique<MaxConsecutiveConstraint>(10000));
    eval.add(std::make_unique<MinRestConstraint>(10000));
    eval.add(std::make_unique<DemandConstraint>(1000, 100));
    eval.add(std::make_unique<ForbiddenSequenceConstraint>(10000));
    eval.add(std::make_unique<PreferenceConstraint>(1));

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    // Apply a sequence of moves, verifying delta at each step.
    std::vector<AssignmentMove> moves = {
        {.employee = 0, .day = 0, .old_shift = -1, .new_shift = 0},
        {.employee = 1, .day = 0, .old_shift = -1, .new_shift = 1},
        {.employee = 0, .day = 1, .old_shift = -1, .new_shift = 0},
        {.employee = 0, .day = 2, .old_shift = -1, .new_shift = 0},  // preferred
        {.employee = 0, .day = 3, .old_shift = -1, .new_shift = 0},  // 4th consec -> violation
        {.employee = 2, .day = 1, .old_shift = -1, .new_shift = 1},
        {.employee = 0, .day = 3, .old_shift = 0, .new_shift = -1},  // undo violation
    };

    for (auto const& m : moves) {
        int before = eval.evaluate(data, sched);
        int delta = eval.evaluate_delta(data, sched, m);
        apply_move(sched, m);
        int after = eval.evaluate(data, sched);

        REQUIRE(delta == after - before);
    }
}
