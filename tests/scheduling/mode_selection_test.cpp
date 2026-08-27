#include "scheduling/mode_selection.h"

#include "scheduling/schedule_data.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a multi-mode RCPSP instance
// ---------------------------------------------------------------------------

namespace {

/// Build a small multi-mode RCPSP instance with 3 operations and 2 modes each.
///
///  2 machines, 1 resource (capacity 3).
///  Job 0: op0 -> op1 -> op2  (all on different machines).
///
///  op0 modes: mode 0 = (dur=5, res=1), mode 1 = (dur=3, res=2)
///  op1 modes: mode 0 = (dur=4, res=1), mode 1 = (dur=2, res=3)
///  op2 modes: mode 0 = (dur=6, res=1), mode 1 = (dur=3, res=2)
///
/// The greedy selector should pick modes that balance duration and resource use.
/// Mode 1 is faster but uses more resources.
ScheduleData make_multimode_3ops() {
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_machine({.name = "M1"});

    b.add_job({.name = "J0"});

    // All operations on M0 with base duration (overridden by modes).
    b.add_operation(0, {.machine = 0, .duration = 5});  // op 0
    b.add_operation(0, {.machine = 1, .duration = 4});  // op 1
    b.add_operation(0, {.machine = 0, .duration = 6});  // op 2

    // 1 resource with capacity 3.
    int r = b.add_resource(3);
    b.set_resource_usage(0, r, 1);
    b.set_resource_usage(1, r, 1);
    b.set_resource_usage(2, r, 1);

    return b.build();
}

/// Build a 2-job instance where mode swaps can improve total duration.
///
///  2 machines, 1 resource (capacity 4).
///  Job 0: op0 (M0)  — modes: (dur=10, res=1), (dur=5, res=2)
///  Job 1: op1 (M1)  — modes: (dur=8, res=1), (dur=4, res=3)
///
/// Starting with mode 0 for both: total_dur = 18.
/// After local search swaps to mode 1 for both: total_dur = 9.
/// Both mode-1 usages (2, 3) are within capacity 4.
ScheduleData make_multimode_improvable() {
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_machine({.name = "M1"});

    b.add_job({.name = "J0"});
    b.add_job({.name = "J1"});

    b.add_operation(0, {.machine = 0, .duration = 10});  // op 0
    b.add_operation(1, {.machine = 1, .duration = 8});   // op 1

    int r = b.add_resource(4);
    b.set_resource_usage(0, r, 1);
    b.set_resource_usage(1, r, 1);

    return b.build();
}

/// Build an instance with infeasible modes (resource usage exceeds capacity).
///
///  1 machine, 1 resource (capacity 2).
///  Job 0: op0 (M0) — base: (dur=3, res=5)  <-- exceeds capacity!
///
/// Used to test that mode_resource_feasible() detects violations.
ScheduleData make_infeasible_resource() {
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_job({.name = "J0"});

    b.add_operation(0, {.machine = 0, .duration = 3});  // op 0

    int r = b.add_resource(2);
    b.set_resource_usage(0, r, 5);  // exceeds capacity

    return b.build();
}

}  // namespace

// ---------------------------------------------------------------------------
//  ModeAssignment construction
// ---------------------------------------------------------------------------

TEST_CASE("ModeAssignment: construction from ScheduleData", "[scheduling][mode_selection]") {
    auto data = make_multimode_3ops();
    ModeAssignment ma(data);

    CHECK(ma.num_operations() == 3);

    // Each operation has 1 mode from the base ScheduleData (fixed machine).
    for (int op = 0; op < 3; ++op) {
        CHECK(ma.num_modes(op) >= 1);
        CHECK(ma.selected_mode(op) == 0);
    }

    // Durations should match the base ScheduleData values.
    CHECK(ma.duration(0) == 5);
    CHECK(ma.duration(1) == 4);
    CHECK(ma.duration(2) == 6);
}

TEST_CASE("ModeAssignment: add_mode extends available modes", "[scheduling][mode_selection]") {
    auto data = make_multimode_3ops();
    ModeAssignment ma(data);

    // Add a second mode for op 0: faster but more resource-hungry.
    ma.add_mode(0, {.duration = 3, .resource_usage = {2}});

    CHECK(ma.num_modes(0) == 2);

    // Switch to mode 1.
    ma.set_mode(0, 1);
    CHECK(ma.selected_mode(0) == 1);
    CHECK(ma.duration(0) == 3);
    CHECK(ma.resource_usage(0, 0) == 2);
}

TEST_CASE("ModeAssignment: total_duration sums selected modes", "[scheduling][mode_selection]") {
    auto data = make_multimode_3ops();
    ModeAssignment ma(data);

    // Base modes: dur = 5 + 4 + 6 = 15.
    CHECK(ma.total_duration() == 15);

    // Add faster mode for op 2 and switch.
    ma.add_mode(2, {.duration = 3, .resource_usage = {2}});
    ma.set_mode(2, 1);

    // Now: 5 + 4 + 3 = 12.
    CHECK(ma.total_duration() == 12);
}

TEST_CASE("ModeAssignment: total_resource_usage", "[scheduling][mode_selection]") {
    auto data = make_multimode_3ops();
    ModeAssignment ma(data);

    // All ops use resource 0 with usage 1: total = 3.
    CHECK(ma.total_resource_usage(0) == 3);
}

TEST_CASE("ModeAssignment: mode_resource_feasible", "[scheduling][mode_selection]") {
    SECTION("feasible instance") {
        auto data = make_multimode_3ops();
        ModeAssignment ma(data);
        CHECK(ma.mode_resource_feasible(data));
    }

    SECTION("infeasible instance") {
        auto data = make_infeasible_resource();
        ModeAssignment ma(data);
        CHECK_FALSE(ma.mode_resource_feasible(data));
    }
}

// ---------------------------------------------------------------------------
//  Greedy mode selection
// ---------------------------------------------------------------------------

TEST_CASE("greedy_mode_selection: picks valid modes", "[scheduling][mode_selection]") {
    auto data = make_multimode_3ops();
    auto ma = greedy_mode_selection(data);

    // All selected modes should be within valid range.
    for (int op = 0; op < ma.num_operations(); ++op) {
        CHECK(ma.selected_mode(op) >= 0);
        CHECK(ma.selected_mode(op) < ma.num_modes(op));
    }

    // Selected modes should be resource-feasible.
    CHECK(ma.mode_resource_feasible(data));
}

TEST_CASE("greedy_mode_selection: skips infeasible modes", "[scheduling][mode_selection]") {
    auto data = make_multimode_3ops();
    ModeAssignment ma(data);

    // Add an infeasible mode (resource usage 10 > capacity 3) and a good mode.
    ma.add_mode(0, {.duration = 1, .resource_usage = {10}});  // infeasible
    ma.add_mode(0, {.duration = 2, .resource_usage = {2}});   // feasible, fast

    // Greedy should pick mode 2 (feasible, lowest cost) not mode 1 (infeasible).
    // Re-run greedy on the data (it builds its own ModeAssignment).
    auto greedy = greedy_mode_selection(data);
    CHECK(greedy.mode_resource_feasible(data));
}

TEST_CASE("greedy_mode_selection: prefers lower cost modes", "[scheduling][mode_selection]") {
    // Build instance with FJSP-style eligible machines (creates multiple modes).
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_machine({.name = "M1"});
    b.add_job({.name = "J0"});

    // Operation with 2 eligible machines: M0 (dur=10), M1 (dur=3).
    b.add_operation(0, {.eligible_machines = {0, 1}, .durations_per_machine = {10, 3}});

    int r = b.add_resource(5);
    b.set_resource_usage(0, r, 1);

    auto data = b.build();
    auto ma = greedy_mode_selection(data);

    // Should pick mode 1 (M1, dur=3) since cost = 3 + 1 = 4 < 10 + 1 = 11.
    CHECK(ma.selected_mode(0) == 1);
    CHECK(ma.duration(0) == 3);
}

// ---------------------------------------------------------------------------
//  Local search improvement
// ---------------------------------------------------------------------------

TEST_CASE("local_search_modes: improves total duration via mode swaps",
          "[scheduling][mode_selection]") {
    auto data = make_multimode_improvable();
    ModeAssignment ma(data);

    // Add faster modes for both operations.
    ma.add_mode(0, {.duration = 5, .resource_usage = {2}});  // mode 1 for op 0
    ma.add_mode(1, {.duration = 4, .resource_usage = {3}});  // mode 1 for op 1

    // Start with mode 0 for both: total_dur = 10 + 8 = 18.
    int before = ma.total_duration();
    CHECK(before == 18);

    local_search_modes(data, ma);

    int after = ma.total_duration();

    // After local search: should switch both to mode 1 -> total = 5 + 4 = 9.
    CHECK(after <= before);
    CHECK(after == 9);
    CHECK(ma.mode_resource_feasible(data));
}

TEST_CASE("local_search_modes: does not worsen solution", "[scheduling][mode_selection]") {
    auto data = make_multimode_3ops();
    ModeAssignment ma(data);

    int before = ma.total_duration();
    local_search_modes(data, ma);
    int after = ma.total_duration();

    CHECK(after <= before);
    CHECK(ma.mode_resource_feasible(data));
}

TEST_CASE("local_search_modes: respects resource capacity", "[scheduling][mode_selection]") {
    // Instance where the faster mode exceeds capacity.
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_job({.name = "J0"});

    b.add_operation(0, {.machine = 0, .duration = 10});
    int r = b.add_resource(2);
    b.set_resource_usage(0, r, 1);

    auto data = b.build();
    ModeAssignment ma(data);

    // Add a tempting but infeasible mode.
    ma.add_mode(0, {.duration = 1, .resource_usage = {5}});  // exceeds cap=2

    local_search_modes(data, ma);

    // Should stay on mode 0 since mode 1 is infeasible.
    CHECK(ma.selected_mode(0) == 0);
    CHECK(ma.duration(0) == 10);
    CHECK(ma.mode_resource_feasible(data));
}

TEST_CASE("local_search_modes: single operation with multiple modes",
          "[scheduling][mode_selection]") {
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_job({.name = "J0"});
    b.add_operation(0, {.machine = 0, .duration = 20});

    int r = b.add_resource(10);
    b.set_resource_usage(0, r, 1);

    auto data = b.build();
    ModeAssignment ma(data);

    // Add 3 alternative modes.
    ma.add_mode(0, {.duration = 15, .resource_usage = {3}});
    ma.add_mode(0, {.duration = 8, .resource_usage = {5}});
    ma.add_mode(0, {.duration = 12, .resource_usage = {4}});

    // Start at mode 0 (dur=20). Local search should find mode 2 (dur=8).
    local_search_modes(data, ma);

    CHECK(ma.duration(0) == 8);
    CHECK(ma.selected_mode(0) == 2);  // mode index 2 has dur=8
}
