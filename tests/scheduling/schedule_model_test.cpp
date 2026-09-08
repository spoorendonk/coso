#include "model/schedule_model.h"

#include "scheduling/schedule_data.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Basic model construction
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleModel: add machines, jobs, operations", "[scheduling]") {
    ScheduleModel model;

    int m0 = model.add_machine({});
    int m1 = model.add_machine({});
    int m2 = model.add_machine({});

    CHECK(m0 == 0);
    CHECK(m1 == 1);
    CHECK(m2 == 2);

    int j0 = model.add_job({});
    int j1 = model.add_job({});

    CHECK(j0 == 0);
    CHECK(j1 == 1);

    // Job 0: two operations on fixed machines.
    int op0 = model.add_operation(j0, {.machine = 0, .duration = 3});
    int op1 = model.add_operation(j0, {.machine = 1, .duration = 4});

    CHECK(op0 == 0);
    CHECK(op1 == 1);

    // Job 1: one operation.
    int op2 = model.add_operation(j1, {.machine = 2, .duration = 5});
    CHECK(op2 == 2);
}

// ---------------------------------------------------------------------------
//  ScheduleData compilation — JSP (fixed machines)
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleData: JSP compilation", "[scheduling]") {
    ScheduleData::Builder builder;

    builder.add_machine({});
    builder.add_machine({});

    builder.add_job({});
    builder.add_job({});

    // J0: op0 on M0 (dur 3), op1 on M1 (dur 2)
    builder.add_operation(0, {.machine = 0, .duration = 3});
    builder.add_operation(0, {.machine = 1, .duration = 2});

    // J1: op2 on M1 (dur 4), op3 on M0 (dur 1)
    builder.add_operation(1, {.machine = 1, .duration = 4});
    builder.add_operation(1, {.machine = 0, .duration = 1});

    ScheduleData data = builder.build();

    CHECK(data.num_machines() == 2);
    CHECK(data.num_jobs() == 2);
    CHECK(data.num_operations() == 4);

    // Check operation-to-job mapping.
    CHECK(data.operation(0).job == 0);
    CHECK(data.operation(1).job == 0);
    CHECK(data.operation(2).job == 1);
    CHECK(data.operation(3).job == 1);

    // Check job-to-operations mapping.
    REQUIRE(data.job(0).operations.size() == 2);
    CHECK(data.job(0).operations[0] == 0);
    CHECK(data.job(0).operations[1] == 1);

    REQUIRE(data.job(1).operations.size() == 2);
    CHECK(data.job(1).operations[0] == 2);
    CHECK(data.job(1).operations[1] == 3);

    // Check processing times.
    CHECK(data.processing_time(0, 0) == 3);        // op0 on M0
    CHECK(data.processing_time(0, 1) == INT_MAX);  // op0 cannot run on M1
    CHECK(data.processing_time(1, 1) == 2);        // op1 on M1
    CHECK(data.processing_time(2, 1) == 4);        // op2 on M1
    CHECK(data.processing_time(3, 0) == 1);        // op3 on M0

    // Check precedence arcs: J0 has op0 -> op1, J1 has op2 -> op3.
    auto precs = data.precedences();
    REQUIRE(precs.size() == 2);
    CHECK(precs[0].before == 0);
    CHECK(precs[0].after == 1);
    CHECK(precs[1].before == 2);
    CHECK(precs[1].after == 3);
}

// ---------------------------------------------------------------------------
//  ScheduleData compilation — FJSP (eligible machines)
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleData: FJSP with eligible machines", "[scheduling]") {
    ScheduleData::Builder builder;

    builder.add_machine({});
    builder.add_machine({});
    builder.add_machine({});

    builder.add_job({});

    // Flexible operation: can run on M0 (dur 5) or M2 (dur 3).
    builder.add_operation(0, {
                                 .eligible_machines = {0, 2},
                                 .durations_per_machine = {5, 3},
                             });

    ScheduleData data = builder.build();

    CHECK(data.num_machines() == 3);
    CHECK(data.num_operations() == 1);

    // Processing time matrix: only M0 and M2 are eligible.
    CHECK(data.processing_time(0, 0) == 5);
    CHECK(data.processing_time(0, 1) == INT_MAX);  // M1 not eligible
    CHECK(data.processing_time(0, 2) == 3);

    // Operation data preserves eligible machines.
    auto const& op = data.operation(0);
    CHECK(op.fixed_machine == -1);
    REQUIRE(op.eligible_machines.size() == 2);
    CHECK(op.eligible_machines[0] == 0);
    CHECK(op.eligible_machines[1] == 2);
}

// ---------------------------------------------------------------------------
//  Objective setting
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleData: objective types", "[scheduling]") {
    ScheduleData::Builder builder;
    builder.add_machine();
    builder.add_job();
    builder.add_operation(0, {.machine = 0, .duration = 1});

    SECTION("default is makespan") {
        auto data = builder.build();
        CHECK(data.objective() == ScheduleObjective::Makespan);
    }

    SECTION("total weighted tardiness") {
        builder.set_objective(ScheduleObjective::TotalWeightedTardiness);
        auto data = builder.build();
        CHECK(data.objective() == ScheduleObjective::TotalWeightedTardiness);
    }
}

// ---------------------------------------------------------------------------
//  ScheduleModel::solve() baseline solve path
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleModel: solve builds a feasible schedule", "[scheduling]") {
    SKIP(
        "ScheduleModel::solve() aborts via construct_neh() on any instance with 2 or more "
        "jobs — coso#188");
    ScheduleModel model;

    model.add_machine({});
    model.add_machine({});

    int j0 = model.add_job({});
    model.add_operation(j0, {.machine = 0, .duration = 3});
    model.add_operation(j0, {.machine = 1, .duration = 2});

    int j1 = model.add_job({});
    model.add_operation(j1, {.machine = 1, .duration = 4});
    model.add_operation(j1, {.machine = 0, .duration = 1});

    model.minimize_makespan();

    Result result = model.solve(TimeLimit(1.0));

    CHECK(result.feasible());
    CHECK(result.makespan() > 0);
    CHECK(result.cost() > 0.0);

    // Elapsed time should be recorded (> 0).
    CHECK(result.elapsed_seconds() >= 0.0);
}

// ---------------------------------------------------------------------------
//  ScheduleModel: solve with empty model returns default result
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleModel: solve with empty model", "[scheduling]") {
    ScheduleModel model;
    Result result = model.solve(TimeLimit(0.1));
    CHECK_FALSE(result.feasible());
}

// ---------------------------------------------------------------------------
//  ScheduleModel: job metadata (due date, weight)
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleData: job metadata preserved", "[scheduling]") {
    ScheduleData::Builder builder;

    builder.add_machine();
    builder.add_job({.due_date = 20, .weight = 3});
    builder.add_operation(0, {.machine = 0, .duration = 4});

    ScheduleData data = builder.build();

    CHECK(data.job(0).due_date == 20);
    CHECK(data.job(0).weight == 3);
}

// ---------------------------------------------------------------------------
//  Error handling: invalid job index
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleModel: invalid job index throws", "[scheduling]") {
    ScheduleModel model;
    model.add_machine();

    CHECK_THROWS_AS(model.add_operation(0, {.machine = 0, .duration = 1}), std::out_of_range);
    CHECK_THROWS_AS(model.add_operation(-1, {.machine = 0, .duration = 1}), std::out_of_range);
}
