#include "rostering/construction.h"

#include "rostering/cost_evaluator.h"
#include "rostering/rostering_data.h"
#include "rostering/rostering_solution.h"

#include <catch2/catch_test_macros.hpp>

#include <climits>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small test instance
//    3 employees, 2 shift types (Day/Night), 7-day horizon
// ---------------------------------------------------------------------------

namespace {

RosteringData make_small_instance() {
    RosteringData data;

    // Shift types: Day (08-16, 8h) and Night (22-06, 8h).
    data.shift_types = {
        {.name = "Day", .duration_minutes = 480},
        {.name = "Night", .duration_minutes = 480},
    };

    // 3 employees with default constraints.
    data.employees = {
        {.name = "Alice", .skills = {"nurse"}, .max_consecutive_days = 5},
        {.name = "Bob", .skills = {"nurse"}, .max_consecutive_days = 5},
        {.name = "Carol", .skills = {"nurse", "senior"}, .max_consecutive_days = 5},
    };

    data.horizon = 7;

    // Demand: 1 nurse per day shift, 1 per night shift, every day.
    for (int d = 0; d < 7; ++d) {
        data.add_demand(0, d, {.min_employees = 1, .max_employees = 2, .required_skill = ""});
        data.add_demand(1, d, {.min_employees = 1, .max_employees = 1, .required_skill = ""});
    }

    return data;
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
//  FFD construction
// ---------------------------------------------------------------------------

TEST_CASE("construct_ffd: small instance meets demand", "[rostering][construction]") {
    auto data = make_small_instance();
    RosteringCostEvaluator evaluator(data);

    auto sol = construct_ffd(data, evaluator);

    // All demand should be satisfied: 0 demand cost.
    REQUIRE(sol.demand_cost() == 0);
}

TEST_CASE("construct_ffd: solution is feasible", "[rostering][construction]") {
    auto data = make_small_instance();
    RosteringCostEvaluator evaluator(data);

    auto sol = construct_ffd(data, evaluator);

    // No hard constraint violations.
    REQUIRE(sol.is_feasible());
}

TEST_CASE("construct_ffd: respects unavailability", "[rostering][construction]") {
    auto data = make_small_instance();
    // Alice unavailable on days 0-2.
    for (int d = 0; d < 3; ++d) {
        data.unavailabilities.insert(RosteringData::unavail_key(0, d));
    }

    RosteringCostEvaluator evaluator(data);
    auto sol = construct_ffd(data, evaluator);

    // Alice should not be assigned on days 0-2.
    for (int d = 0; d < 3; ++d) {
        REQUIRE(sol.get(0, d) == -1);
    }

    // Solution should still be feasible.
    REQUIRE(sol.is_feasible());
}

TEST_CASE("construct_ffd: respects consecutive day limit", "[rostering][construction]") {
    auto data = make_small_instance();
    // Tighten consecutive days to 3 for all employees.
    for (auto& emp : data.employees) {
        emp.max_consecutive_days = 3;
    }

    RosteringCostEvaluator evaluator(data);
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

TEST_CASE("construct_ffd: respects forbidden sequences", "[rostering][construction]") {
    auto data = make_small_instance();
    // Forbid Night -> Day sequence.
    data.forbidden_sequences = {{1, 0}};

    RosteringCostEvaluator evaluator(data);
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

TEST_CASE("construct_ffd: skill-based demand", "[rostering][construction]") {
    auto data = make_small_instance();
    // Require "senior" skill for day shift on days 0-4 only (Carol can cover 5).
    for (int d = 0; d < 7; ++d) {
        data.demand.erase(RosteringData::demand_key(0, d));
    }
    for (int d = 0; d < 5; ++d) {
        data.add_demand(0, d, {.min_employees = 1, .max_employees = 2, .required_skill = "senior"});
    }
    // Days 5-6: no skill requirement (any employee).
    for (int d = 5; d < 7; ++d) {
        data.add_demand(0, d, {.min_employees = 1, .max_employees = 2, .required_skill = ""});
    }

    RosteringCostEvaluator evaluator(data);
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

TEST_CASE("construct_greedy: small instance meets demand", "[rostering][construction]") {
    auto data = make_small_instance();
    RosteringCostEvaluator evaluator(data);

    auto sol = construct_greedy(data, evaluator);

    REQUIRE(sol.demand_cost() == 0);
}

TEST_CASE("construct_greedy: solution is feasible", "[rostering][construction]") {
    auto data = make_small_instance();
    RosteringCostEvaluator evaluator(data);

    auto sol = construct_greedy(data, evaluator);

    REQUIRE(sol.is_feasible());
}

TEST_CASE("construct_greedy: respects unavailability", "[rostering][construction]") {
    auto data = make_small_instance();
    for (int d = 0; d < 3; ++d) {
        data.unavailabilities.insert(RosteringData::unavail_key(0, d));
    }

    RosteringCostEvaluator evaluator(data);
    auto sol = construct_greedy(data, evaluator);

    for (int d = 0; d < 3; ++d) {
        REQUIRE(sol.get(0, d) == -1);
    }

    REQUIRE(sol.is_feasible());
}

TEST_CASE("construct_greedy: cost is better than empty solution", "[rostering][construction]") {
    auto data = make_small_instance();
    RosteringCostEvaluator evaluator(data);

    RosteringSolution empty(data, evaluator);
    auto sol = construct_greedy(data, evaluator);

    // Constructed solution should have lower cost than empty.
    REQUIRE(sol.cost() < empty.cost());
}

TEST_CASE("construct_ffd: cost is better than empty solution", "[rostering][construction]") {
    auto data = make_small_instance();
    RosteringCostEvaluator evaluator(data);

    RosteringSolution empty(data, evaluator);
    auto sol = construct_ffd(data, evaluator);

    REQUIRE(sol.cost() < empty.cost());
}

// ---------------------------------------------------------------------------
//  Hard working-time and weekend bounds (#246)
//
//  Each instance has one employee and soft cover that rewards working more
//  than the bound allows, so a construction that ignored the bound would
//  build a cheaper-looking roster that violates it.
// ---------------------------------------------------------------------------

namespace {

using Construction = RosteringSolution (*)(RosteringData const&, RosteringCostEvaluator const&);

/// One employee, one 480-minute shift, soft cover of one on every day.
RosteringData make_single_employee(int horizon) {
    RosteringData data;
    data.shift_types = {{.name = "Day", .duration_minutes = 480}};
    data.employees = {{.name = "Alice", .max_consecutive_days = horizon}};
    data.horizon = horizon;
    for (int d = 0; d < horizon; ++d) {
        data.add_demand(0, d, {.min_employees = 1, .max_employees = 1});
    }
    return data;
}

int worked_minutes(RosteringData const& data, RosteringSolution const& sol, int e) {
    int total = 0;
    for (int d = 0; d < data.horizon; ++d) {
        if (sol.get(e, d) >= 0) {
            total += data.shift_types[sol.get(e, d)].duration_minutes;
        }
    }
    return total;
}

}  // anonymous namespace

TEST_CASE("constructions respect hard max_weekends", "[rostering][construction]") {
    // Two weekends (days 5-6 and 12-13); cover wants both, the bound allows one.
    auto data = make_single_employee(14);
    data.employees[0].max_weekends = 1;
    RosteringCostEvaluator evaluator(data);

    for (Construction construct : {&construct_ffd, &construct_greedy}) {
        auto sol = construct(data, evaluator);
        bool const first = sol.get(0, 5) >= 0 || sol.get(0, 6) >= 0;
        bool const second = sol.get(0, 12) >= 0 || sol.get(0, 13) >= 0;
        CHECK(static_cast<int>(first) + static_cast<int>(second) <= 1);
        CHECK(evaluator.weekend_violation_cost(sol.schedule()) == 0);
        CHECK(sol.is_feasible());
    }
}

TEST_CASE("constructions respect hard max_total_minutes", "[rostering][construction]") {
    // Cover wants all 7 days; the bound allows 3 shifts.
    auto data = make_single_employee(7);
    data.employees[0].max_total_minutes = 3 * 480;
    RosteringCostEvaluator evaluator(data);

    for (Construction construct : {&construct_ffd, &construct_greedy}) {
        auto sol = construct(data, evaluator);
        CHECK(worked_minutes(data, sol, 0) == 3 * 480);
        CHECK(evaluator.working_time_violation_cost(sol.schedule()) == 0);
        CHECK(sol.is_feasible());
    }
}

TEST_CASE("constructions respect hard min_total_minutes", "[rostering][construction]") {
    // Cover wants day 0 only and prices every other shift as over-cover; the
    // bound demands 3 shifts.
    auto data = make_single_employee(7);
    for (int d = 1; d < 7; ++d) {
        data.demand.erase(RosteringData::demand_key(0, d));
        data.add_demand(0, d, {.min_employees = 0, .max_employees = 0});
    }
    data.employees[0].min_total_minutes = 3 * 480;
    RosteringCostEvaluator evaluator(data);

    for (Construction construct : {&construct_ffd, &construct_greedy}) {
        auto sol = construct(data, evaluator);
        CHECK(sol.get(0, 0) == 0);
        CHECK(worked_minutes(data, sol, 0) == 3 * 480);
        CHECK(evaluator.working_time_violation_cost(sol.schedule()) == 0);
        CHECK(sol.is_feasible());
    }
}

TEST_CASE("constructions report an unreachable min_total_minutes as infeasible",
          "[rostering][construction]") {
    // max_total_minutes caps the employee below the minimum, so no roster
    // satisfies both; the construction must not pretend one does.
    auto data = make_single_employee(7);
    data.employees[0].min_total_minutes = 5 * 480;
    data.employees[0].max_total_minutes = 4 * 480;
    RosteringCostEvaluator evaluator(data);

    for (Construction construct : {&construct_ffd, &construct_greedy}) {
        auto sol = construct(data, evaluator);
        CHECK(worked_minutes(data, sol, 0) <= 4 * 480);
        CHECK_FALSE(sol.is_feasible());
    }
}

TEST_CASE("is_feasible_assignment enforces max_weekends and max_total_minutes",
          "[rostering][construction]") {
    // Shared with replanning's construct_unlocked, so this pins both.
    auto data = make_single_employee(14);
    data.employees[0].max_weekends = 1;
    std::vector<std::vector<int>> schedule(1, std::vector<int>(14, -1));

    SECTION("a worked weekend's other day stays open, a second weekend does not") {
        schedule[0][5] = 0;  // Saturday of weekend 0.
        CHECK(is_feasible_assignment(data, schedule, 0, 6, 0));
        CHECK_FALSE(is_feasible_assignment(data, schedule, 0, 12, 0));
        CHECK_FALSE(is_feasible_assignment(data, schedule, 0, 13, 0));
        CHECK(is_feasible_assignment(data, schedule, 0, 8, 0));  // A weekday.
    }

    SECTION("an assignment past max_total_minutes is refused") {
        data.employees[0].max_total_minutes = 2 * 480;
        schedule[0][0] = 0;
        CHECK(is_feasible_assignment(data, schedule, 0, 1, 0));
        schedule[0][1] = 0;
        CHECK_FALSE(is_feasible_assignment(data, schedule, 0, 2, 0));
    }
}
