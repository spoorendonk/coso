#include "assignment/constraints/automaton.h"

#include "assignment/assignment_data.h"
#include "assignment/constraints/constraint.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small test instance
//    3 employees, 3 shift types (Day/Night/Evening), 7-day horizon
// ---------------------------------------------------------------------------

namespace {

AssignmentData make_instance() {
    AssignmentData data;

    data.shift_types = {
        {.name = "Day", .start_hour = 8, .end_hour = 16, .duration_hours = 0},
        {.name = "Night", .start_hour = 22, .end_hour = 6, .duration_hours = 0},
        {.name = "Evening", .start_hour = 16, .end_hour = 22, .duration_hours = 0},
    };

    data.employees = {
        {.name = "Alice",
         .skills = {"nurse"},
         .max_hours_per_week = 40,
         .max_consecutive_days = 7,
         .min_rest_hours = 11},
        {.name = "Bob",
         .skills = {"nurse"},
         .max_hours_per_week = 40,
         .max_consecutive_days = 7,
         .min_rest_hours = 11},
        {.name = "Carol",
         .skills = {"nurse"},
         .max_hours_per_week = 40,
         .max_consecutive_days = 7,
         .min_rest_hours = 11},
    };

    data.horizon = 7;
    return data;
}

/// Apply a move to a schedule in-place.
void apply_move(std::vector<std::vector<int>>& schedule, AssignmentMove const& move) {
    schedule[move.employee][move.day] = move.new_shift;
}

}  // anonymous namespace

// ===========================================================================
//  build_max_consecutive_dfa
// ===========================================================================

TEST_CASE("AutomatonConstraint: max 2 consecutive night shifts — no violation",
          "[assignment][automaton]") {
    auto data = make_instance();
    int const NIGHT = 1;
    auto dfa = build_max_consecutive_dfa(NIGHT, 2, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    // 2 consecutive nights: should be fine.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = NIGHT;
    sched[0][1] = NIGHT;

    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("AutomatonConstraint: max 2 consecutive night shifts — violation on 3rd",
          "[assignment][automaton]") {
    auto data = make_instance();
    int const NIGHT = 1;
    auto dfa = build_max_consecutive_dfa(NIGHT, 2, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    // 3 consecutive nights: 3rd day enters rejecting sink.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = NIGHT;
    sched[0][1] = NIGHT;
    sched[0][2] = NIGHT;

    REQUIRE(c.evaluate(data, sched) == 10000);
}

TEST_CASE("AutomatonConstraint: max 2 consecutive — 4 in a row counts 2 violations",
          "[assignment][automaton]") {
    auto data = make_instance();
    int const NIGHT = 1;
    auto dfa = build_max_consecutive_dfa(NIGHT, 2, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    // 4 consecutive nights: days 2 and 3 are in rejecting state.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = NIGHT;
    sched[0][1] = NIGHT;
    sched[0][2] = NIGHT;
    sched[0][3] = NIGHT;

    REQUIRE(c.evaluate(data, sched) == 2 * 10000);
}

TEST_CASE("AutomatonConstraint: max consecutive resets after break", "[assignment][automaton]") {
    auto data = make_instance();
    int const NIGHT = 1;
    auto dfa = build_max_consecutive_dfa(NIGHT, 2, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    // N, N, off, N, N: no violation (break resets the counter).
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = NIGHT;
    sched[0][1] = NIGHT;
    // day 2 is off
    sched[0][3] = NIGHT;
    sched[0][4] = NIGHT;

    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("AutomatonConstraint: max consecutive — other shifts don't count",
          "[assignment][automaton]") {
    auto data = make_instance();
    int const NIGHT = 1;
    int const DAY = 0;
    auto dfa = build_max_consecutive_dfa(NIGHT, 2, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    // N, N, Day, N: the Day shift resets the counter.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = NIGHT;
    sched[0][1] = NIGHT;
    sched[0][2] = DAY;
    sched[0][3] = NIGHT;

    REQUIRE(c.evaluate(data, sched) == 0);
}

// ===========================================================================
//  build_forbidden_pattern_dfa
// ===========================================================================

TEST_CASE("AutomatonConstraint: forbidden Night->Day pattern", "[assignment][automaton]") {
    auto data = make_instance();
    int const DAY = 0;
    int const NIGHT = 1;
    auto dfa = build_forbidden_pattern_dfa({NIGHT, DAY}, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    // Night followed by Day: violation.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = NIGHT;
    sched[0][1] = DAY;

    REQUIRE(c.evaluate(data, sched) == 10000);
}

TEST_CASE("AutomatonConstraint: forbidden pattern — Day->Night is OK", "[assignment][automaton]") {
    auto data = make_instance();
    int const DAY = 0;
    int const NIGHT = 1;
    auto dfa = build_forbidden_pattern_dfa({NIGHT, DAY}, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    // Day followed by Night: no violation.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = DAY;
    sched[0][1] = NIGHT;

    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("AutomatonConstraint: forbidden 3-symbol pattern", "[assignment][automaton]") {
    auto data = make_instance();
    int const DAY = 0;
    int const NIGHT = 1;
    int const EVE = 2;

    // Forbid Night -> Evening -> Day.
    auto dfa = build_forbidden_pattern_dfa({NIGHT, EVE, DAY}, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    // Exact forbidden pattern: violation.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = NIGHT;
    sched[0][1] = EVE;
    sched[0][2] = DAY;
    REQUIRE(c.evaluate(data, sched) == 10000);

    // Partial match (Night, Evening, Night): no violation.
    sched[0][2] = NIGHT;
    REQUIRE(c.evaluate(data, sched) == 0);
}

TEST_CASE("AutomatonConstraint: forbidden pattern — multiple occurrences",
          "[assignment][automaton]") {
    auto data = make_instance();
    int const NIGHT = 1;
    int const DAY = 0;
    auto dfa = build_forbidden_pattern_dfa({NIGHT, DAY}, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    // Two occurrences of Night->Day.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = NIGHT;
    sched[0][1] = DAY;  // First violation.
    sched[0][2] = NIGHT;
    sched[0][3] = DAY;  // Second violation.

    REQUIRE(c.evaluate(data, sched) == 2 * 10000);
}

// ===========================================================================
//  Delta evaluation
// ===========================================================================

TEST_CASE("AutomatonConstraint: delta matches full — max consecutive", "[assignment][automaton]") {
    auto data = make_instance();
    int const NIGHT = 1;
    auto dfa = build_max_consecutive_dfa(NIGHT, 2, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = NIGHT;
    sched[0][1] = NIGHT;

    // Adding a 3rd night: should create violation.
    AssignmentMove move{.employee = 0, .day = 2, .old_shift = -1, .new_shift = NIGHT};
    int delta = c.evaluate_delta(data, sched, move);

    auto after = sched;
    apply_move(after, move);
    int expected = c.evaluate(data, after) - c.evaluate(data, sched);

    REQUIRE(delta == expected);
    REQUIRE(delta == 10000);
}

TEST_CASE("AutomatonConstraint: delta matches full — forbidden pattern",
          "[assignment][automaton]") {
    auto data = make_instance();
    int const DAY = 0;
    int const NIGHT = 1;
    auto dfa = build_forbidden_pattern_dfa({NIGHT, DAY}, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = NIGHT;

    // Adding Day after Night: creates forbidden sequence.
    AssignmentMove move{.employee = 0, .day = 1, .old_shift = -1, .new_shift = DAY};
    int delta = c.evaluate_delta(data, sched, move);

    auto after = sched;
    apply_move(after, move);
    int expected = c.evaluate(data, after) - c.evaluate(data, sched);

    REQUIRE(delta == expected);
    REQUIRE(delta == 10000);
}

TEST_CASE("AutomatonConstraint: delta — removing violation", "[assignment][automaton]") {
    auto data = make_instance();
    int const NIGHT = 1;
    auto dfa = build_max_consecutive_dfa(NIGHT, 2, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    // Start with 3 consecutive nights (1 violation).
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = NIGHT;
    sched[0][1] = NIGHT;
    sched[0][2] = NIGHT;

    REQUIRE(c.evaluate(data, sched) == 10000);

    // Remove the middle night: breaks the run.
    AssignmentMove move{.employee = 0, .day = 1, .old_shift = NIGHT, .new_shift = -1};
    int delta = c.evaluate_delta(data, sched, move);

    auto after = sched;
    apply_move(after, move);
    int expected = c.evaluate(data, after) - c.evaluate(data, sched);

    REQUIRE(delta == expected);
    REQUIRE(delta == -10000);
}

TEST_CASE("AutomatonConstraint: delta — sequential moves stay accurate",
          "[assignment][automaton]") {
    auto data = make_instance();
    int const DAY = 0;
    int const NIGHT = 1;
    auto dfa = build_max_consecutive_dfa(NIGHT, 2, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    std::vector<AssignmentMove> moves = {
        {.employee = 0, .day = 0, .old_shift = -1, .new_shift = NIGHT},
        {.employee = 0, .day = 1, .old_shift = -1, .new_shift = NIGHT},
        {.employee = 0, .day = 2, .old_shift = -1, .new_shift = NIGHT},  // violation
        {.employee = 1, .day = 0, .old_shift = -1, .new_shift = NIGHT},
        {.employee = 0, .day = 2, .old_shift = NIGHT, .new_shift = DAY},  // undo violation
        {.employee = 0, .day = 3, .old_shift = -1, .new_shift = NIGHT},
        {.employee = 0, .day = 4, .old_shift = -1, .new_shift = NIGHT},
    };

    for (auto const& m : moves) {
        int before = c.evaluate(data, sched);
        int delta = c.evaluate_delta(data, sched, m);
        apply_move(sched, m);
        int after_cost = c.evaluate(data, sched);

        REQUIRE(delta == after_cost - before);
    }
}

// ===========================================================================
//  Multiple employees
// ===========================================================================

TEST_CASE("AutomatonConstraint: violations across multiple employees", "[assignment][automaton]") {
    auto data = make_instance();
    int const NIGHT = 1;
    auto dfa = build_max_consecutive_dfa(NIGHT, 2, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));

    // Alice: 3 consecutive nights (1 violation).
    sched[0][0] = NIGHT;
    sched[0][1] = NIGHT;
    sched[0][2] = NIGHT;

    // Bob: 4 consecutive nights (2 violations).
    sched[1][0] = NIGHT;
    sched[1][1] = NIGHT;
    sched[1][2] = NIGHT;
    sched[1][3] = NIGHT;

    // Carol: 2 consecutive nights (no violation).
    sched[2][0] = NIGHT;
    sched[2][1] = NIGHT;

    REQUIRE(c.evaluate(data, sched) == 3 * 10000);
}

// ===========================================================================
//  Integration with ConstraintEvaluator
// ===========================================================================

TEST_CASE("AutomatonConstraint: works in ConstraintEvaluator", "[assignment][automaton]") {
    auto data = make_instance();
    int const NIGHT = 1;

    ConstraintEvaluator eval;
    eval.add(std::make_unique<AutomatonConstraint>(
        build_max_consecutive_dfa(NIGHT, 2, data.num_shift_types()), 10000));

    REQUIRE(eval.size() == 1);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    REQUIRE(eval.evaluate(data, sched) == 0);

    sched[0][0] = NIGHT;
    sched[0][1] = NIGHT;
    sched[0][2] = NIGHT;

    REQUIRE(eval.evaluate(data, sched) == 10000);

    auto bd = eval.breakdown(data, sched);
    REQUIRE(bd.size() == 1);
    REQUIRE(bd[0].first == "Automaton");
    REQUIRE(bd[0].second == 10000);
}

// ===========================================================================
//  DFA with "off" in forbidden pattern
// ===========================================================================

TEST_CASE("AutomatonConstraint: forbidden pattern including off day", "[assignment][automaton]") {
    auto data = make_instance();
    int const NIGHT = 1;

    // Forbid Night -> off -> Night (no isolated off days between nights).
    auto dfa = build_forbidden_pattern_dfa({NIGHT, -1, NIGHT}, data.num_shift_types());
    AutomatonConstraint c(std::move(dfa), 10000);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = NIGHT;
    // day 1 is off
    sched[0][2] = NIGHT;

    REQUIRE(c.evaluate(data, sched) == 10000);

    // Night -> off -> off -> Night: no violation (off-off breaks the pattern).
    std::vector<std::vector<int>> sched2(3, std::vector<int>(7, -1));
    sched2[0][0] = NIGHT;
    // days 1, 2 are off
    sched2[0][3] = NIGHT;

    REQUIRE(c.evaluate(data, sched2) == 0);
}
