#include "model/rostering_model.h"

#include "rostering/parsers.h"
#include "rostering/rostering_data.h"

#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <cstddef>
#include <string>
#include <vector>

using namespace coso;

// ===========================================================================
//  RosteringData unit tests
// ===========================================================================

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
    int morning = model.add_shift_type({.name = "Morning", .duration_minutes = 480});
    int evening = model.add_shift_type({.name = "Evening", .duration_minutes = 480});
    int night = model.add_shift_type({.name = "Night", .duration_minutes = 480});

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

    model.add_forbidden_sequence(0, 0);  // No back-to-back day shifts.

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

TEST_CASE("RosteringModel: solve returns result with elapsed time", "[rostering]") {
    RosteringModel model;
    model.add_shift_type({.name = "Morning"});
    model.add_employee({.name = "Nurse A"});
    model.set_horizon(7);
    for (int d = 0; d < 7; ++d) {
        model.add_demand(0, d, {.min_employees = 1});
    }

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
    for (int d = 0; d < 4; ++d) {
        model.add_demand(0, d, {.min_employees = 1, .max_employees = 1});
    }

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
    int day = a.add_shift_type({.name = "Day", .duration_minutes = 480});
    a.add_employee({.name = "Alice"});
    for (int d = 0; d < 3; ++d) {
        a.add_demand(day, d, DemandParams{.min_employees = 1});
    }

    RosteringModel b = a;

    auto ra = a.solve(TimeLimit(0.1));
    auto rb = b.solve(TimeLimit(0.1));

    REQUIRE(ra.feasible());
    REQUIRE(rb.feasible() == ra.feasible());
    REQUIRE(rb.cost() == ra.cost());
    REQUIRE(rb.roster().size() == ra.roster().size());
}

// ===========================================================================
//  Round-trip: schedulingbenchmarks.org NRP (#203 v1 scope ruling)
// ===========================================================================

namespace {

// ---------------------------------------------------------------------------
//  Round-trip mechanism
// ---------------------------------------------------------------------------
//
//  `RosteringModel` compiles its declaration into a `RosteringData` inside
//  `solve()` and never hands it back, and `RosteringData` has no `operator==`.
//  The helpers below therefore duplicate, on purpose, (a) the compile block of
//  `src/model/rostering_model.cpp` `solve()` and (b) an equality operator that
//  does not exist.  Both are known duplication and both die with the backend
//  dispatch of #176; until then they can drift from `solve()`, so a change to
//  the compile order there belongs in `compile_declared` too.

/// Replay a declaration into a `RosteringData`, in the order `solve()` does:
/// shift types, employees, horizon, demand, forbidden sequences, preferences,
/// unavailabilities.
RosteringData compile_declared(RosteringModel const& m) {
    RosteringData data;

    data.shift_types.reserve(static_cast<size_t>(m.num_shift_types()));
    for (int s = 0; s < m.num_shift_types(); ++s) {
        auto const& st = m.shift_type(s);
        data.shift_types.push_back({
            .name = st.name,
            .duration_minutes = st.duration_minutes,
        });
    }

    data.employees.reserve(static_cast<size_t>(m.num_employees()));
    for (int e = 0; e < m.num_employees(); ++e) {
        auto const& emp = m.employee(e);
        data.employees.push_back({
            .name = emp.name,
            .skills = emp.skills,
            .max_consecutive_days = emp.max_consecutive_days,
            .max_weekends = emp.max_weekends,
            .min_total_minutes = emp.min_total_minutes,
            .max_total_minutes = emp.max_total_minutes,
        });
    }

    data.horizon = m.horizon();

    for (auto const& d : m.demands()) {
        data.demand[RosteringData::demand_key(d.shift_type, d.day)] = {
            .min_employees = d.params.min_employees,
            .max_employees = d.params.max_employees,
            .required_skill = d.params.required_skill,
        };
    }

    data.forbidden_sequences.reserve(m.forbidden_sequences().size());
    for (auto const& [first, second] : m.forbidden_sequences()) {
        data.forbidden_sequences.push_back({first, second});
    }

    data.preferences.reserve(m.preferences().size());
    for (auto const& p : m.preferences()) {
        data.preferences.push_back({
            .employee = p.employee,
            .day = p.day,
            .shift_type = p.shift_type,
            .weight = p.weight,
        });
    }

    for (auto const& u : m.unavailabilities()) {
        data.unavailabilities.insert(RosteringData::unavail_key(u.employee, u.day));
    }

    return data;
}

/// Compare every field `RosteringData` carries.  `demand` and
/// `unavailabilities` are unordered containers, so each is swept in both
/// directions: equal size alone would miss a key swap, and one-way containment
/// alone would miss a key only the other side has.
///
/// Every aggregate below is taken apart with a structured binding rather than
/// by member access, and that is load-bearing rather than stylistic.  A
/// structured binding names each non-static member of an aggregate exactly
/// once, so adding a field to `RosteringData` or to one of its nested structs
/// stops this file compiling and points at the list to extend.  Without that,
/// the pinning cases below would keep passing for a field that had just become
/// representable -- they assert two declarations compile *identically*, and a
/// field this hand-written comparison does not look at is identical by
/// omission.
void check_same_rostering_data(RosteringData const& a, RosteringData const& b) {
    auto const& [a_shifts, a_emps, a_horizon, a_demand, a_forbidden, a_prefs, a_unavail] = a;
    auto const& [b_shifts, b_emps, b_horizon, b_demand, b_forbidden, b_prefs, b_unavail] = b;

    REQUIRE(a_shifts.size() == b_shifts.size());
    REQUIRE(a_emps.size() == b_emps.size());
    CHECK(a_horizon == b_horizon);

    for (size_t s = 0; s < a_shifts.size(); ++s) {
        INFO("shift type " << s);
        auto const& [a_name, a_duration] = a_shifts[s];
        auto const& [b_name, b_duration] = b_shifts[s];
        CHECK(a_name == b_name);
        CHECK(a_duration == b_duration);
    }

    for (size_t e = 0; e < a_emps.size(); ++e) {
        INFO("employee " << e);
        auto const& [a_name, a_skills, a_consec, a_weekends, a_min_min, a_max_min] = a_emps[e];
        auto const& [b_name, b_skills, b_consec, b_weekends, b_min_min, b_max_min] = b_emps[e];
        CHECK(a_name == b_name);
        CHECK(a_skills == b_skills);
        CHECK(a_consec == b_consec);
        CHECK(a_weekends == b_weekends);
        CHECK(a_min_min == b_min_min);
        CHECK(a_max_min == b_max_min);
    }

    // Demand, both directions.
    REQUIRE(a_demand.size() == b_demand.size());
    for (auto const& [key, dem] : a_demand) {
        INFO("demand key " << key << " of a");
        auto it = b_demand.find(key);
        REQUIRE(it != b_demand.end());
        auto const& [a_min, a_max, a_skill] = dem;
        auto const& [b_min, b_max, b_skill] = it->second;
        CHECK(a_min == b_min);
        CHECK(a_max == b_max);
        CHECK(a_skill == b_skill);
    }
    for (auto const& [key, dem] : b_demand) {
        INFO("demand key " << key << " of b");
        auto it = a_demand.find(key);
        REQUIRE(it != a_demand.end());
        auto const& [b_min, b_max, b_skill] = dem;
        auto const& [a_min, a_max, a_skill] = it->second;
        CHECK(a_min == b_min);
        CHECK(a_max == b_max);
        CHECK(a_skill == b_skill);
    }

    REQUIRE(a_forbidden.size() == b_forbidden.size());
    for (size_t i = 0; i < a_forbidden.size(); ++i) {
        INFO("forbidden sequence " << i);
        CHECK(a_forbidden[i] == b_forbidden[i]);
    }

    REQUIRE(a_prefs.size() == b_prefs.size());
    for (size_t i = 0; i < a_prefs.size(); ++i) {
        INFO("preference " << i);
        auto const& [a_emp, a_day, a_shift, a_weight] = a_prefs[i];
        auto const& [b_emp, b_day, b_shift, b_weight] = b_prefs[i];
        CHECK(a_emp == b_emp);
        CHECK(a_day == b_day);
        CHECK(a_shift == b_shift);
        CHECK(a_weight == b_weight);
    }

    // Unavailabilities, both directions.
    REQUIRE(a_unavail.size() == b_unavail.size());
    for (auto key : a_unavail) {
        INFO("unavailability key " << key << " of a");
        CHECK(b_unavail.count(key) == 1);
    }
    for (auto key : b_unavail) {
        INFO("unavailability key " << key << " of b");
        CHECK(a_unavail.count(key) == 1);
    }
}

}  // namespace

TEST_CASE("A parsed SB-NRP instance is declarable field for field", "[rostering][roundtrip]") {
    // A schedulingbenchmarks.org NRP instance written to the format documented
    // at src/rostering/parsers.h and src/rostering/parsers.cpp:52-80.  It is
    // deliberately not shared with tests/rostering/parser_test.cpp: a header
    // would couple the parser's own coverage to this audit's, and either
    // file's literal must be able to fail on its own.
    //
    // What it exercises, and why each is here:
    //   * a 14-day horizon, so days 5, 6, 12 and 13 are two real weekends
    //     under RosteringData::is_weekend_day -- MaxWeekends is inert without
    //     one, and >= 400 of Instance1's optimum of 607 comes from that field;
    //   * two shift types with a forbidden succession, "D|N" after N, which is
    //     how every verified format spells inter-shift rest;
    //   * employee A in the **positional** STAFF dialect, which is what real
    //     schedulingbenchmarks.org files use and which parser_test.cpp does not
    //     cover, and employee Bea in the key=value dialect;
    //   * MaxTotalMinutes / MinTotalMinutes / MaxWeekends on both, the three
    //     #203 EXTEND additions;
    //   * hard days off, shift-on and shift-off requests, and a full cover
    //     block with the SB under=100 / over=1 weights.
    std::string const content = R"(
SECTION_HORIZON
14

SECTION_SHIFTS
D,480
N,480,D|N

SECTION_STAFF
A,D=10|N=4,4320,3390,5,2,2,1
Bea,MaxShifts=10,MaxTotalMinutes=4320,MinTotalMinutes=3390,MaxConsecutiveShifts=4,MinConsecutiveShifts=2,MinConsecutiveDaysOff=2,MaxWeekends=2

SECTION_DAYS_OFF
A,3
Bea,5,6

SECTION_SHIFT_ON_REQUESTS
A,1,D,2
Bea,8,N,3

SECTION_SHIFT_OFF_REQUESTS
A,12,N,3
Bea,13,D,1

SECTION_COVER
0,D,3,100,1
0,N,1,100,1
1,D,3,100,1
1,N,1,100,1
2,D,3,100,1
2,N,1,100,1
3,D,3,100,1
3,N,1,100,1
4,D,3,100,1
4,N,1,100,1
5,D,2,100,1
5,N,1,100,1
6,D,2,100,1
6,N,1,100,1
7,D,3,100,1
7,N,1,100,1
8,D,3,100,1
8,N,1,100,1
9,D,3,100,1
9,N,1,100,1
10,D,3,100,1
10,N,1,100,1
11,D,3,100,1
11,N,1,100,1
12,D,2,100,1
12,N,1,100,1
13,D,2,100,1
13,N,1,100,1
)";

    // -- Step 1 -------------------------------------------------------------
    // The parser reads the literal as written.  A mis-typed literal must fail
    // here, not silently weaken the comparison below.
    RosteringData parsed = parse_nrp(content);

    CHECK(parsed.horizon == 14);

    REQUIRE(parsed.num_shift_types() == 2);
    CHECK(parsed.shift_types[0].name == "D");
    CHECK(parsed.shift_types[0].duration_minutes == 480);
    CHECK(parsed.shift_types[1].name == "N");
    CHECK(parsed.shift_types[1].duration_minutes == 480);

    REQUIRE(parsed.num_employees() == 2);
    // A: the positional dialect.  Columns 2..7 are MaxTotalMinutes,
    // MinTotalMinutes, MaxConsecutiveShifts, MinConsecutiveShifts,
    // MinConsecutiveDaysOff, MaxWeekends.
    CHECK(parsed.employees[0].name == "A");
    CHECK(parsed.employees[0].max_total_minutes == 4320);
    CHECK(parsed.employees[0].min_total_minutes == 3390);
    CHECK(parsed.employees[0].max_consecutive_days == 5);
    CHECK(parsed.employees[0].max_weekends == 1);
    CHECK(parsed.employees[0].skills.empty());  // SB-NRP carries no skills
    // Bea: the key=value dialect, same three EXTEND fields.
    CHECK(parsed.employees[1].name == "Bea");
    CHECK(parsed.employees[1].max_total_minutes == 4320);
    CHECK(parsed.employees[1].min_total_minutes == 3390);
    CHECK(parsed.employees[1].max_consecutive_days == 4);
    CHECK(parsed.employees[1].max_weekends == 2);

    // "N,480,D|N": neither D nor N may follow N, in pipe order.
    REQUIRE(parsed.forbidden_sequences.size() == 2);
    CHECK(parsed.forbidden_sequences[0] == std::vector<int>{1, 0});
    CHECK(parsed.forbidden_sequences[1] == std::vector<int>{1, 1});

    // Days off are hard: A on day 3, Bea across the first weekend.
    REQUIRE(parsed.unavailabilities.size() == 3);
    CHECK(parsed.is_unavailable(0, 3));
    CHECK(parsed.is_unavailable(1, 5));
    CHECK(parsed.is_unavailable(1, 6));
    CHECK_FALSE(parsed.is_unavailable(0, 5));

    // The sign convention, which is the one thing a reader must not have to
    // infer: parse_nrp keeps a SECTION_SHIFT_ON_REQUESTS weight as written
    // (src/rostering/parsers.cpp:277) and **negates** a
    // SECTION_SHIFT_OFF_REQUESTS one (:303), because
    // RosteringCostEvaluator::preference_cost does `cost -= weight`.  A
    // benchmark penalty is therefore a negative weight on the model side too.
    REQUIRE(parsed.preferences.size() == 4);
    CHECK(parsed.preferences[0].employee == 0);  // A wants D on day 1
    CHECK(parsed.preferences[0].day == 1);
    CHECK(parsed.preferences[0].shift_type == 0);
    CHECK(parsed.preferences[0].weight == 2);  // on-request: positive
    CHECK(parsed.preferences[1].employee == 1);
    CHECK(parsed.preferences[1].day == 8);
    CHECK(parsed.preferences[1].shift_type == 1);
    CHECK(parsed.preferences[1].weight == 3);
    CHECK(parsed.preferences[2].employee == 0);  // A avoids N on day 12
    CHECK(parsed.preferences[2].day == 12);
    CHECK(parsed.preferences[2].shift_type == 1);
    CHECK(parsed.preferences[2].weight == -3);  // off-request: negated
    CHECK(parsed.preferences[3].employee == 1);
    CHECK(parsed.preferences[3].day == 13);
    CHECK(parsed.preferences[3].shift_type == 0);
    CHECK(parsed.preferences[3].weight == -1);

    // Cover: 2 shift types x 14 days, weekday D = 3, weekend D = 2, N = 1.
    REQUIRE(parsed.demand.size() == 28);
    CHECK(parsed.get_demand(0, 0).min_employees == 3);
    CHECK(parsed.get_demand(0, 5).min_employees == 2);   // Saturday
    CHECK(parsed.get_demand(0, 13).min_employees == 2);  // second Sunday
    CHECK(parsed.get_demand(1, 0).min_employees == 1);
    // SECTION_COVER carries no maximum and no skill, so both stay inert.
    CHECK(parsed.get_demand(0, 0).max_employees == INT_MAX);
    CHECK(parsed.get_demand(0, 0).required_skill.empty());

    // -- Step 2 -------------------------------------------------------------
    // The same instance declared through RosteringModel.
    RosteringModel model;
    model.set_horizon(14);

    int const day = model.add_shift_type({.name = "D", .duration_minutes = 480});
    int const night = model.add_shift_type({.name = "N", .duration_minutes = 480});

    int const a = model.add_employee({
        .name = "A",
        .max_consecutive_days = 5,
        .max_weekends = 1,
        .min_total_minutes = 3390,
        .max_total_minutes = 4320,
    });
    int const bea = model.add_employee({
        .name = "Bea",
        .max_consecutive_days = 4,
        .max_weekends = 2,
        .min_total_minutes = 3390,
        .max_total_minutes = 4320,
    });

    // "D|N" after N, in the parser's pipe order.
    model.add_forbidden_sequence(night, day);
    model.add_forbidden_sequence(night, night);

    model.add_unavailability(a, 3);
    model.add_unavailability(bea, 5);
    model.add_unavailability(bea, 6);

    // The sign convention above, spelled out rather than left as a bare minus
    // sign in an argument list.
    auto shift_on = [&](int employee, int d, int shift, int weight) {
        model.add_preference(employee, d, shift, weight);
    };
    auto shift_off = [&](int employee, int d, int shift, int weight) {
        model.add_preference(employee, d, shift, -weight);
    };
    shift_on(a, 1, day, 2);
    shift_on(bea, 8, night, 3);
    shift_off(a, 12, night, 3);
    shift_off(bea, 13, day, 1);

    for (int d = 0; d < 14; ++d) {
        model.add_demand(day, d, {.min_employees = RosteringData::is_weekend_day(d) ? 2 : 3});
        model.add_demand(night, d, {.min_employees = 1});
    }

    // -- Step 3 -------------------------------------------------------------
    // Exact equality.  Every field parse_nrp writes into a RosteringData has a
    // RosteringModel setter behind it; the SB-NRP fields it does *not* write
    // are pinned below rather than tolerated here.
    check_same_rostering_data(parsed, compile_declared(model));
}

// ===========================================================================
//  Gap pinning: SB-NRP fields parse_nrp discards
// ===========================================================================
//
//  A discarded field never reaches a RosteringData, so the field-by-field
//  comparison above cannot see it and prose cannot fail.  Each section below
//  parses two literals that differ **only** in the discarded field and asserts
//  they compile identically.
//
//  That is only a real pin because check_same_rostering_data destructures
//  every aggregate with structured bindings: making one of these fields
//  representable means adding a member to RosteringData, which stops this file
//  compiling until the comparison list is extended -- at which point the
//  section below starts failing, as it should.  Without that, a pin would keep
//  passing for a field the comparison had never been taught to look at.

TEST_CASE("Discarded SB-NRP fields stay discarded", "[rostering][roundtrip][pinning]") {
    // The pinning base: one week, two shift types, one employee in the
    // positional dialect, one cover line.  Every variant below is this literal
    // with a single field changed.
    auto positional = [](std::string const& staff, std::string const& cover) {
        return "SECTION_HORIZON\n7\n\n"
               "SECTION_SHIFTS\nD,480\nN,480,D|N\n\n"
               "SECTION_STAFF\n" +
               staff + "\n\nSECTION_COVER\n" + cover + "\n";
    };

    SECTION("SECTION_COVER under- and over-cover weights") {
        // src/rostering/parsers.cpp:308-329 reads tokens[0..2] -- day, shift,
        // requirement -- and never touches tokens[3] (UnderWeight) or
        // tokens[4] (OverWeight).  RosteringData::Demand has nowhere to put
        // them and RosteringModel cannot declare them.
        //
        // This is the #203 ruling's decisive finding and it belongs to
        // **#229**: all 26824 cover lines across SB-NRP 1-24 carry
        // under=100, over=1, while RosteringModel::solve() hardcodes
        // understaffing=1000 / overstaffing=100.  Until #229, a v1 run solves
        // a differently-weighted problem and cannot be scored against any
        // published value.
        std::string const staff = "A,D=7|N=7,3360,1440,5,2,2,1";
        RosteringData sb_weights = parse_nrp(positional(staff, "0,D,2,100,1"));
        RosteringData inverted = parse_nrp(positional(staff, "0,D,2,1,1000"));
        // The weights are not even required: the parser accepts a three-token
        // cover line, so their absence is indistinguishable from any value.
        RosteringData omitted = parse_nrp(positional(staff, "0,D,2"));

        check_same_rostering_data(sb_weights, inverted);
        check_same_rostering_data(sb_weights, omitted);
    }

    SECTION("STAFF MaxShifts per shift type") {
        // Positional column 1, "D=7|N=7": a per-employee cap on how many
        // shifts of each type may be assigned.  parsers.cpp:196-206 skips
        // tokens[1] entirely, and the key=value branch (:209-226) matches only
        // MaxConsecutiveShifts, MaxTotalMinutes, MinTotalMinutes and
        // MaxWeekends, so "MaxShifts=" falls through both dialects.  Deferred
        // by the #203 ruling ("per-employee per-shift-type assignment caps").
        std::string const cover = "0,D,2,100,1";
        check_same_rostering_data(parse_nrp(positional("A,D=7|N=7,3360,1440,5,2,2,1", cover)),
                                  parse_nrp(positional("A,D=1,3360,1440,5,2,2,1", cover)));

        std::string const named =
            "A,MaxShifts=7,MaxTotalMinutes=3360,MinTotalMinutes=1440,MaxConsecutiveShifts=5,"
            "MinConsecutiveShifts=2,MinConsecutiveDaysOff=2,MaxWeekends=1";
        std::string const named_capped =
            "A,MaxShifts=1,MaxTotalMinutes=3360,MinTotalMinutes=1440,MaxConsecutiveShifts=5,"
            "MinConsecutiveShifts=2,MinConsecutiveDaysOff=2,MaxWeekends=1";
        check_same_rostering_data(parse_nrp(positional(named, cover)),
                                  parse_nrp(positional(named_capped, cover)));
    }

    SECTION("STAFF MinConsecutiveShifts") {
        // Positional column 5.  parsers.cpp reads column 4
        // (MaxConsecutiveShifts) and column 7 (MaxWeekends) and steps over
        // this one; the key=value branch has no case for it either.  The #203
        // ruling defers it explicitly ("min consecutive working shifts").
        std::string const cover = "0,D,2,100,1";
        check_same_rostering_data(parse_nrp(positional("A,D=7|N=7,3360,1440,5,2,2,1", cover)),
                                  parse_nrp(positional("A,D=7|N=7,3360,1440,5,4,2,1", cover)));

        std::string const named =
            "A,MaxShifts=7,MaxTotalMinutes=3360,MinTotalMinutes=1440,MaxConsecutiveShifts=5,"
            "MinConsecutiveShifts=2,MinConsecutiveDaysOff=2,MaxWeekends=1";
        std::string const named_longer =
            "A,MaxShifts=7,MaxTotalMinutes=3360,MinTotalMinutes=1440,MaxConsecutiveShifts=5,"
            "MinConsecutiveShifts=4,MinConsecutiveDaysOff=2,MaxWeekends=1";
        check_same_rostering_data(parse_nrp(positional(named, cover)),
                                  parse_nrp(positional(named_longer, cover)));
    }

    SECTION("STAFF MinConsecutiveDaysOff") {
        // Positional column 6, discarded by both dialects for the same reason
        // as MinConsecutiveShifts.  Deferred by the #203 ruling ("min
        // consecutive days off").
        std::string const cover = "0,D,2,100,1";
        check_same_rostering_data(parse_nrp(positional("A,D=7|N=7,3360,1440,5,2,2,1", cover)),
                                  parse_nrp(positional("A,D=7|N=7,3360,1440,5,2,5,1", cover)));

        std::string const named =
            "A,MaxShifts=7,MaxTotalMinutes=3360,MinTotalMinutes=1440,MaxConsecutiveShifts=5,"
            "MinConsecutiveShifts=2,MinConsecutiveDaysOff=2,MaxWeekends=1";
        std::string const named_longer =
            "A,MaxShifts=7,MaxTotalMinutes=3360,MinTotalMinutes=1440,MaxConsecutiveShifts=5,"
            "MinConsecutiveShifts=2,MinConsecutiveDaysOff=5,MaxWeekends=1";
        check_same_rostering_data(parse_nrp(positional(named, cover)),
                                  parse_nrp(positional(named_longer, cover)));
    }

    SECTION("'#' comment lines, where an SB-NRP file names itself") {
        // parsers.cpp:116-118 drops them.  The SB-NRP distribution carries the
        // instance name and its section-column legends in exactly those lines,
        // and neither RosteringData nor RosteringModel has an instance name to
        // hold any of it.
        std::string const staff = "A,D=7|N=7,3360,1440,5,2,2,1";
        std::string const bare = positional(staff, "0,D,2,100,1");
        std::string const annotated =
            "# Instance1\n# ID, MaxShifts, MaxTotalMinutes, MinTotalMinutes\n" +
            positional(staff, "0,D,2,100,1");

        check_same_rostering_data(parse_nrp(bare), parse_nrp(annotated));
    }
}
