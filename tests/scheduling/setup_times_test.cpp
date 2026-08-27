#include "scheduling/setup_times.h"

#include "scheduling/schedule_data.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  SetupTimeMatrix basics
// ---------------------------------------------------------------------------

TEST_CASE("SetupTimeMatrix: default construction", "[scheduling][setup]") {
    SetupTimeMatrix stm;
    CHECK(stm.empty());
    CHECK(stm.num_operations() == 0);
    CHECK(stm.num_machines() == 0);
    // Querying out-of-range returns 0.
    CHECK(stm.setup_time(0, 0, 0) == 0);
}

TEST_CASE("SetupTimeMatrix: set and query per-machine", "[scheduling][setup]") {
    SetupTimeMatrix stm(3, 2);  // 3 operations, 2 machines
    CHECK_FALSE(stm.empty());
    CHECK(stm.num_operations() == 3);
    CHECK(stm.num_machines() == 2);

    // Initially all zeros.
    CHECK(stm.setup_time(0, 1, 0) == 0);
    CHECK(stm.setup_time(1, 0, 1) == 0);

    // Set specific setup times.
    stm.set(0, 1, 0, 5);   // op0 -> op1 on machine 0: 5
    stm.set(0, 1, 1, 3);   // op0 -> op1 on machine 1: 3
    stm.set(1, 2, 0, 10);  // op1 -> op2 on machine 0: 10

    CHECK(stm.setup_time(0, 1, 0) == 5);
    CHECK(stm.setup_time(0, 1, 1) == 3);
    CHECK(stm.setup_time(1, 2, 0) == 10);
    CHECK(stm.setup_time(1, 2, 1) == 0);  // not set
    CHECK(stm.setup_time(2, 0, 0) == 0);  // not set
}

TEST_CASE("SetupTimeMatrix: set uniform across machines", "[scheduling][setup]") {
    SetupTimeMatrix stm(3, 3);

    stm.set(0, 2, 7);  // op0 -> op2 on all machines: 7

    CHECK(stm.setup_time(0, 2, 0) == 7);
    CHECK(stm.setup_time(0, 2, 1) == 7);
    CHECK(stm.setup_time(0, 2, 2) == 7);
    CHECK(stm.setup_time(2, 0, 0) == 0);  // reverse direction not set
}

TEST_CASE("SetupTimeMatrix: overwrite values", "[scheduling][setup]") {
    SetupTimeMatrix stm(2, 1);

    stm.set(0, 1, 0, 5);
    CHECK(stm.setup_time(0, 1, 0) == 5);

    stm.set(0, 1, 0, 8);
    CHECK(stm.setup_time(0, 1, 0) == 8);
}

TEST_CASE("SetupTimeMatrix: out-of-range queries return 0", "[scheduling][setup]") {
    SetupTimeMatrix stm(2, 2);
    stm.set(0, 1, 0, 5);

    // Out-of-range indices should return 0 (not crash).
    CHECK(stm.setup_time(-1, 0, 0) == 0);
    CHECK(stm.setup_time(0, 5, 0) == 0);
    CHECK(stm.setup_time(0, 0, 9) == 0);
}

TEST_CASE("SetupTimeMatrix: out-of-range set throws", "[scheduling][setup]") {
    SetupTimeMatrix stm(2, 2);
    CHECK_THROWS(stm.set(-1, 0, 0, 5));
    CHECK_THROWS(stm.set(0, 5, 0, 5));
    CHECK_THROWS(stm.set(0, 0, 9, 5));
}

// ---------------------------------------------------------------------------
//  Integration with ScheduleData
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleData: setup times via Builder", "[scheduling][setup]") {
    ScheduleData::Builder builder;

    builder.add_machine({.name = "M0"});
    builder.add_machine({.name = "M1"});
    builder.add_job({.name = "J0"});
    builder.add_job({.name = "J1"});

    // J0: op0 on M0, J1: op1 on M0
    builder.add_operation(0, {.machine = 0, .duration = 3});
    builder.add_operation(1, {.machine = 0, .duration = 4});

    // Setup time from op0 -> op1 on machine 0.
    builder.set_setup_time(0, 1, 0, 5);

    auto data = builder.build();

    CHECK(data.has_setup_times());
    CHECK(data.setup_time(0, 1, 0) == 5);
    CHECK(data.setup_time(1, 0, 0) == 0);
    CHECK(data.setup_time(0, 1, 1) == 0);
}

TEST_CASE("ScheduleData: uniform setup times via Builder", "[scheduling][setup]") {
    ScheduleData::Builder builder;

    builder.add_machine({.name = "M0"});
    builder.add_machine({.name = "M1"});
    builder.add_job({.name = "J0"});
    builder.add_job({.name = "J1"});

    builder.add_operation(0, {.machine = 0, .duration = 3});
    builder.add_operation(1, {.machine = 1, .duration = 4});

    // Uniform setup time.
    builder.set_setup_time(0, 1, 7);

    auto data = builder.build();

    CHECK(data.has_setup_times());
    CHECK(data.setup_time(0, 1, 0) == 7);
    CHECK(data.setup_time(0, 1, 1) == 7);
}

TEST_CASE("ScheduleData: no setup times by default", "[scheduling][setup]") {
    ScheduleData::Builder builder;
    builder.add_machine({.name = "M0"});
    builder.add_job({.name = "J0"});
    builder.add_operation(0, {.machine = 0, .duration = 3});

    auto data = builder.build();

    CHECK_FALSE(data.has_setup_times());
    CHECK(data.setup_time(0, 0, 0) == 0);
}
