#include "rostering/constraints/constraint.h"

#include "rostering/rostering_data.h"

#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <memory>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small test instance
//    3 employees, 2 shift types (Day/Night), 7-day horizon
// ---------------------------------------------------------------------------

namespace {

RosteringData make_instance() {
    RosteringData data;

    data.shift_types = {
        {.name = "Day", .duration_minutes = 480},
        {.name = "Night", .duration_minutes = 480},
    };

    data.employees = {
        {.name = "Alice", .skills = {"nurse"}, .max_consecutive_days = 5},
        {.name = "Bob", .skills = {"nurse"}, .max_consecutive_days = 5},
        {.name = "Carol", .skills = {"nurse", "senior"}, .max_consecutive_days = 5},
    };

    data.horizon = 7;

    for (int d = 0; d < 7; ++d) {
        data.demand[RosteringData::demand_key(0, d)] = {
            .min_employees = 1, .max_employees = 2, .required_skill = ""};
        data.demand[RosteringData::demand_key(1, d)] = {
            .min_employees = 1, .max_employees = 1, .required_skill = ""};
    }

    return data;
}

/// Apply a move to a schedule in-place.
void apply_move(std::vector<std::vector<int>>& schedule, RosteringMove const& move) {
    schedule[move.employee][move.day] = move.new_shift;
}

}  // anonymous namespace

// ===========================================================================
//  MaxConsecutiveConstraint
// ===========================================================================

TEST_CASE("MaxConsecutiveConstraint: no violation", "[rostering][constraint]") {
    auto data = make_instance();
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

TEST_CASE("MaxConsecutiveConstraint: violation on 4th day", "[rostering][constraint]") {
    auto data = make_instance();
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

TEST_CASE("MaxConsecutiveConstraint: delta matches full", "[rostering][constraint]") {
    auto data = make_instance();
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }

    MaxConsecutiveConstraint c(10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 0;
    sched[0][1] = 0;
    sched[0][2] = 0;

    // Assign 4th day: should create exactly 1 violation.
    RosteringMove move{.employee = 0, .day = 3, .old_shift = -1, .new_shift = 0};
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

TEST_CASE("DemandConstraint: empty schedule has understaffing", "[rostering][constraint]") {
    auto data = make_instance();
    DemandConstraint c(1000, 100);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    // 7 days * (1 understaffed day + 1 understaffed night) = 14 * 1000
    REQUIRE(c.evaluate(data, sched) == 14 * 1000);
}

TEST_CASE("DemandConstraint: satisfied demand", "[rostering][constraint]") {
    auto data = make_instance();
    DemandConstraint c(1000, 100);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    for (int d = 0; d < 7; ++d) {
        sched[0][d] = 0;  // Alice -> Day
        sched[1][d] = 1;  // Bob -> Night
    }

    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("DemandConstraint: overstaffing", "[rostering][constraint]") {
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

TEST_CASE("DemandConstraint: delta matches full", "[rostering][constraint]") {
    auto data = make_instance();
    DemandConstraint c(1000, 100);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    // Assign Alice to Day on day 0.
    RosteringMove move{.employee = 0, .day = 0, .old_shift = -1, .new_shift = 0};
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

TEST_CASE("ForbiddenSequenceConstraint: no forbidden sequences", "[rostering][constraint]") {
    auto data = make_instance();
    data.forbidden_sequences.clear();
    ForbiddenSequenceConstraint c(10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 1;
    sched[0][1] = 0;

    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("ForbiddenSequenceConstraint: Night->Day forbidden", "[rostering][constraint]") {
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

TEST_CASE("ForbiddenSequenceConstraint: delta matches full", "[rostering][constraint]") {
    auto data = make_instance();
    data.forbidden_sequences = {{1, 0}};  // Night -> Day
    ForbiddenSequenceConstraint c(10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 1;  // Night on day 0

    // Assign Day on day 1 -> creates forbidden Night->Day.
    RosteringMove move{.employee = 0, .day = 1, .old_shift = -1, .new_shift = 0};
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

TEST_CASE("PreferenceConstraint: satisfied preference", "[rostering][constraint]") {
    auto data = make_instance();
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };
    PreferenceConstraint c(1);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 0;  // Alice gets preferred Day shift

    REQUIRE(c.evaluate(data, sched) == -5);
}

TEST_CASE("PreferenceConstraint: unsatisfied preference", "[rostering][constraint]") {
    auto data = make_instance();
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };
    PreferenceConstraint c(1);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 1;  // Night instead of preferred Day

    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("PreferenceConstraint: delta matches full", "[rostering][constraint]") {
    auto data = make_instance();
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };
    PreferenceConstraint c(1);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    // Assign preferred shift.
    RosteringMove move{.employee = 0, .day = 0, .old_shift = -1, .new_shift = 0};
    int delta = c.evaluate_delta(data, sched, move);

    auto after = sched;
    apply_move(after, move);
    int expected = c.evaluate(data, after) - c.evaluate(data, sched);

    REQUIRE(delta == expected);
    REQUIRE(delta == -5);

    // Now remove the preferred shift.
    RosteringMove move2{.employee = 0, .day = 0, .old_shift = 0, .new_shift = -1};
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

TEST_CASE("ConstraintEvaluator: composite evaluate", "[rostering][constraint]") {
    auto data = make_instance();
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }
    data.forbidden_sequences = {{1, 0}};
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };

    ConstraintEvaluator eval;
    eval.add(std::make_unique<MaxConsecutiveConstraint>(10000));
    eval.add(std::make_unique<DemandConstraint>(1000, 100));
    eval.add(std::make_unique<ForbiddenSequenceConstraint>(10000));
    eval.add(std::make_unique<PreferenceConstraint>(1));

    REQUIRE(eval.size() == 4);

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

TEST_CASE("ConstraintEvaluator: composite delta matches full", "[rostering][constraint]") {
    auto data = make_instance();
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }
    data.forbidden_sequences = {{1, 0}};
    data.preferences = {
        {.employee = 0, .day = 0, .shift_type = 0, .weight = 5},
    };

    ConstraintEvaluator eval;
    eval.add(std::make_unique<MaxConsecutiveConstraint>(10000));
    eval.add(std::make_unique<DemandConstraint>(1000, 100));
    eval.add(std::make_unique<ForbiddenSequenceConstraint>(10000));
    eval.add(std::make_unique<PreferenceConstraint>(1));

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    RosteringMove move{.employee = 0, .day = 0, .old_shift = -1, .new_shift = 0};
    int delta = eval.evaluate_delta(data, sched, move);

    auto after = sched;
    apply_move(after, move);
    int expected = eval.evaluate(data, after) - eval.evaluate(data, sched);

    REQUIRE(delta == expected);
}

TEST_CASE("ConstraintEvaluator: breakdown reports per-constraint costs",
          "[rostering][constraint]") {
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

TEST_CASE("Constraint delta: sequential moves stay accurate", "[rostering][constraint]") {
    auto data = make_instance();
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }
    data.forbidden_sequences = {{1, 0}};
    data.preferences = {
        {.employee = 0, .day = 2, .shift_type = 0, .weight = 10},
    };

    ConstraintEvaluator eval;
    eval.add(std::make_unique<MaxConsecutiveConstraint>(10000));
    eval.add(std::make_unique<DemandConstraint>(1000, 100));
    eval.add(std::make_unique<ForbiddenSequenceConstraint>(10000));
    eval.add(std::make_unique<PreferenceConstraint>(1));

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    // Apply a sequence of moves, verifying delta at each step.
    std::vector<RosteringMove> moves = {
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
