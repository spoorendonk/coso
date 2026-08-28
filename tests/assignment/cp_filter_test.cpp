#include "assignment/cp_filter.h"

#include "assignment/assignment_data.h"
#include "assignment/constraints/constraint.h"

#include <catch2/catch_test_macros.hpp>

#include <climits>

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

}  // anonymous namespace

// ===========================================================================
//  Max consecutive pruning
// ===========================================================================

TEST_CASE("CPFilter: max consecutive prunes gap in long run", "[assignment][cp_filter]") {
    auto data = make_instance();
    data.max_consecutive_shifts = 3;
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }

    CPFilter filter(data);

    // Alice works days 0,1,2 (gap on 3) and 4,5.
    // Gap at day 3 with 3 before and 2 after: 3+1+2 = 6 > 3 -> prune all.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 0;
    sched[0][1] = 0;
    sched[0][2] = 0;
    // day 3 is unassigned
    sched[0][4] = 0;
    sched[0][5] = 0;

    filter.propagate(sched);

    // Day 3 for Alice should be fully pruned.
    REQUIRE(filter.domain(0, 3) == 0);
    REQUIRE(filter.is_feasible(0, 3, 0) == false);
    REQUIRE(filter.is_feasible(0, 3, 1) == false);

    // Day 6 for Alice should still be open (only 2 before, 0 after = 3 <= 3).
    REQUIRE(filter.domain_size(0, 6) == 2);
}

TEST_CASE("CPFilter: max consecutive allows within limit", "[assignment][cp_filter]") {
    auto data = make_instance();
    data.max_consecutive_shifts = 3;
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }

    CPFilter filter(data);

    // Alice works days 0 and 2 only. Gap at day 1: before=1, after=1 -> 3 <= 3.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 0;
    sched[0][2] = 0;

    filter.propagate(sched);

    // Day 1 should still be feasible.
    REQUIRE(filter.domain_size(0, 1) == 2);
    REQUIRE(filter.is_feasible(0, 1, 0) == true);
    REQUIRE(filter.is_feasible(0, 1, 1) == true);
}

// ===========================================================================
//  Min rest pruning
// ===========================================================================

TEST_CASE("CPFilter: min rest prunes early shift after late shift", "[assignment][cp_filter]") {
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

    CPFilter filter(data);

    // Late on day 0 (ends 20:00).
    // Early on day 1 (starts 06:00): rest = (24-20) + 6 = 10 < 11 -> pruned.
    // Late on day 1 (starts 12:00): rest = (24-20) + 12 = 16 >= 11 -> ok.
    std::vector<std::vector<int>> sched(1, std::vector<int>(3, -1));
    sched[0][0] = 0;  // Late

    filter.propagate(sched);

    REQUIRE(filter.is_feasible(0, 1, 0) == true);   // Late->Late ok
    REQUIRE(filter.is_feasible(0, 1, 1) == false);  // Late->Early pruned
    REQUIRE(filter.domain_size(0, 1) == 1);
}

TEST_CASE("CPFilter: min rest prunes based on successor shift", "[assignment][cp_filter]") {
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

    CPFilter filter(data);

    // Early on day 2 (starts 06:00).
    // Day 1 -> Early day 2: Late on day 1 ends 20:00, rest = 4+6=10 < 11 -> pruned.
    //                        Early on day 1 ends 14:00, rest = 10+6=16 >= 11 -> ok.
    std::vector<std::vector<int>> sched(1, std::vector<int>(3, -1));
    sched[0][2] = 1;  // Early on day 2

    filter.propagate(sched);

    REQUIRE(filter.is_feasible(0, 1, 0) == false);  // Late then Early: pruned
    REQUIRE(filter.is_feasible(0, 1, 1) == true);   // Early then Early: ok
}

// ===========================================================================
//  Forbidden sequence pruning
// ===========================================================================

TEST_CASE("CPFilter: forbidden sequence prunes completing shift", "[assignment][cp_filter]") {
    auto data = make_instance();
    data.forbidden_sequences = {{1, 0}};  // Night -> Day is forbidden

    CPFilter filter(data);

    // Night on day 0. Day 1 is unassigned.
    // Assigning Day (0) on day 1 completes Night->Day -> prune Day on day 1.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 1;  // Night

    filter.propagate(sched);

    REQUIRE(filter.is_feasible(0, 1, 0) == false);  // Day pruned (completes N->D)
    REQUIRE(filter.is_feasible(0, 1, 1) == true);   // Night ok (N->N not forbidden)
}

TEST_CASE("CPFilter: forbidden sequence prunes first element too", "[assignment][cp_filter]") {
    auto data = make_instance();
    data.forbidden_sequences = {{1, 0}};  // Night -> Day

    CPFilter filter(data);

    // Day on day 1. Day 0 is unassigned.
    // Assigning Night on day 0 completes Night->Day -> prune Night on day 0.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][1] = 0;  // Day

    filter.propagate(sched);

    REQUIRE(filter.is_feasible(0, 0, 1) == false);  // Night pruned
    REQUIRE(filter.is_feasible(0, 0, 0) == true);   // Day ok (D->D not forbidden)
}

// ===========================================================================
//  Demand limit pruning
// ===========================================================================

TEST_CASE("CPFilter: demand limit prunes when at max", "[assignment][cp_filter]") {
    auto data = make_instance();
    // Night max = 1 per day.

    CPFilter filter(data);

    // Bob has Night on day 0. Night max = 1 -> prune Night for others on day 0.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[1][0] = 1;  // Bob -> Night day 0

    filter.propagate(sched);

    // Alice and Carol: Night should be pruned on day 0.
    REQUIRE(filter.is_feasible(0, 0, 1) == false);  // Alice: Night pruned
    REQUIRE(filter.is_feasible(2, 0, 1) == false);  // Carol: Night pruned
    // Day shift still available (max = 2, none assigned).
    REQUIRE(filter.is_feasible(0, 0, 0) == true);
    REQUIRE(filter.is_feasible(2, 0, 0) == true);
}

// ===========================================================================
//  Unavailability pruning
// ===========================================================================

TEST_CASE("CPFilter: unavailability prunes all shifts", "[assignment][cp_filter]") {
    auto data = make_instance();
    data.unavailabilities.insert(AssignmentData::unavail_key(0, 2));

    CPFilter filter(data);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    filter.propagate(sched);

    REQUIRE(filter.domain(0, 2) == 0);
    REQUIRE(filter.is_feasible(0, 2, 0) == false);
    REQUIRE(filter.is_feasible(0, 2, 1) == false);
    // Other days still open.
    REQUIRE(filter.domain_size(0, 0) == 2);
}

// ===========================================================================
//  Filtered moves are indeed infeasible
// ===========================================================================

TEST_CASE("CPFilter: filtered moves violate constraints", "[assignment][cp_filter]") {
    auto data = make_instance();
    data.max_consecutive_shifts = 3;
    for (auto& e : data.employees) {
        e.max_consecutive_days = 3;
    }
    data.forbidden_sequences = {{1, 0}};  // Night -> Day

    CPFilter filter(data);

    // Alice: Day 0-2 assigned, Night on day 4.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    sched[0][0] = 0;
    sched[0][1] = 0;
    sched[0][2] = 0;
    sched[0][4] = 1;  // Night

    filter.propagate(sched);

    // Build some candidate moves for Alice.
    std::vector<AssignmentMove> moves;

    // Day 3: would create 4 consecutive (0,1,2,3) -> pruned if before+1+after > 3.
    // before = 3, after = 0 (day 4 is assigned but day 3 is free).
    // Wait: day 4 IS assigned. before = 3, after = 1 -> 3+1+1 = 5 > 3 -> pruned.
    moves.push_back({.employee = 0, .day = 3, .old_shift = -1, .new_shift = 0});
    moves.push_back({.employee = 0, .day = 3, .old_shift = -1, .new_shift = 1});

    // Day 5: Night on day 4, assign Day on day 5 -> Night->Day forbidden.
    // Also need to check: before=1 (day 4), after=0 -> 2 <= 3: ok for consec.
    moves.push_back({.employee = 0, .day = 5, .old_shift = -1, .new_shift = 0});
    // Night on day 5 after Night on day 4: ok (N->N not forbidden).
    moves.push_back({.employee = 0, .day = 5, .old_shift = -1, .new_shift = 1});

    // Day 6: should be fully open for Alice.
    moves.push_back({.employee = 0, .day = 6, .old_shift = -1, .new_shift = 0});

    // Unassign move (always kept).
    moves.push_back({.employee = 0, .day = 0, .old_shift = 0, .new_shift = -1});

    int removed = filter.filter_moves(moves);

    // Day 3: both shifts pruned (max consecutive). Day 5 shift 0: pruned (forbidden seq).
    // Total removed: 3.
    REQUIRE(removed == 3);

    // Remaining: day 5 Night, day 6 Day, unassign.
    REQUIRE(moves.size() == 3);
}

// ===========================================================================
//  Move filtering reduces search space
// ===========================================================================

TEST_CASE("CPFilter: filter_moves reduces candidate count", "[assignment][cp_filter]") {
    auto data = make_instance();
    data.max_consecutive_shifts = 2;
    for (auto& e : data.employees) {
        e.max_consecutive_days = 2;
    }

    CPFilter filter(data);

    // All employees work days 0 and 1.
    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    for (int e = 0; e < 3; ++e) {
        sched[e][0] = 0;
        sched[e][1] = 0;
    }

    filter.propagate(sched);

    // Generate moves: assign any shift to any employee on day 2.
    // With max_consec=2, before=2, after=0 -> 2+1+0 = 3 > 2 -> all pruned on day 2.
    std::vector<AssignmentMove> moves;
    for (int e = 0; e < 3; ++e) {
        moves.push_back({.employee = e, .day = 2, .old_shift = -1, .new_shift = 0});
        moves.push_back({.employee = e, .day = 2, .old_shift = -1, .new_shift = 1});
    }

    int original = static_cast<int>(moves.size());
    int removed = filter.filter_moves(moves);

    REQUIRE(removed == original);
    REQUIRE(moves.empty());
}

// ===========================================================================
//  Unassign moves pass through
// ===========================================================================

TEST_CASE("CPFilter: unassign moves are never filtered", "[assignment][cp_filter]") {
    auto data = make_instance();
    data.unavailabilities.insert(AssignmentData::unavail_key(0, 0));

    CPFilter filter(data);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    filter.propagate(sched);

    // Even though Alice day 0 is fully pruned, unassign passes through.
    std::vector<AssignmentMove> moves;
    moves.push_back({.employee = 0, .day = 0, .old_shift = 0, .new_shift = -1});

    int removed = filter.filter_moves(moves);
    REQUIRE(removed == 0);
    REQUIRE(moves.size() == 1);
}

// ===========================================================================
//  Combined propagation
// ===========================================================================

TEST_CASE("CPFilter: multiple rules interact correctly", "[assignment][cp_filter]") {
    auto data = make_instance();
    data.max_consecutive_shifts = 5;
    for (auto& e : data.employees) {
        e.max_consecutive_days = 5;
    }
    data.forbidden_sequences = {{1, 0}};  // Night -> Day
    data.unavailabilities.insert(AssignmentData::unavail_key(2, 3));

    CPFilter filter(data);

    std::vector<std::vector<int>> sched(3, std::vector<int>(7, -1));
    // Alice: Night on day 0.
    sched[0][0] = 1;
    // Bob: Night on days 0, 1 only.  Night max = 1 per day.
    sched[1][0] = 1;
    sched[1][1] = 1;

    filter.propagate(sched);

    // Alice day 1: Night->Day forbidden, so Day is pruned.
    // Night on day 1 already taken by Bob (max=1), so Night pruned too.
    REQUIRE(filter.is_feasible(0, 1, 0) == false);  // forbidden seq
    REQUIRE(filter.is_feasible(0, 1, 1) == false);  // demand limit

    // Carol day 3: unavailable -> fully pruned.
    REQUIRE(filter.domain(2, 3) == 0);

    // Bob day 3: fully open (no consecutive run blocking, no demand cap).
    REQUIRE(filter.domain_size(1, 3) == 2);
}
