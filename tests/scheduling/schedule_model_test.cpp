#include <catch2/catch_test_macros.hpp>

#include "model/schedule_model.h"
#include "scheduling/schedule_data.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Basic model construction
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleModel: add machines, jobs, operations", "[scheduling]")
{
    ScheduleModel model;

    int m0 = model.add_machine({.name = "M0"});
    int m1 = model.add_machine({.name = "M1"});
    int m2 = model.add_machine({.name = "M2"});

    CHECK(m0 == 0);
    CHECK(m1 == 1);
    CHECK(m2 == 2);

    int j0 = model.add_job({.name = "Job0"});
    int j1 = model.add_job({.name = "Job1"});

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

TEST_CASE("ScheduleData: JSP compilation", "[scheduling]")
{
    ScheduleData::Builder builder;

    builder.add_machine({.name = "M0"});
    builder.add_machine({.name = "M1"});

    builder.add_job({.name = "J0"});
    builder.add_job({.name = "J1"});

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
    CHECK(data.num_resources() == 0);

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
    CHECK(data.processing_time(0, 0) == 3);    // op0 on M0
    CHECK(data.processing_time(0, 1) == INT_MAX); // op0 cannot run on M1
    CHECK(data.processing_time(1, 1) == 2);    // op1 on M1
    CHECK(data.processing_time(2, 1) == 4);    // op2 on M1
    CHECK(data.processing_time(3, 0) == 1);    // op3 on M0

    // Check precedence arcs: J0 has op0 -> op1, J1 has op2 -> op3.
    auto precs = data.precedences();
    REQUIRE(precs.size() == 2);
    CHECK(precs[0].before == 0);
    CHECK(precs[0].after == 1);
    CHECK(precs[1].before == 2);
    CHECK(precs[1].after == 3);

    // Machine names.
    CHECK(data.machine_name(0) == "M0");
    CHECK(data.machine_name(1) == "M1");

    // Job metadata.
    CHECK(data.job(0).name == "J0");
    CHECK(data.job(1).name == "J1");
}

// ---------------------------------------------------------------------------
//  ScheduleData compilation — FJSP (eligible machines)
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleData: FJSP with eligible machines", "[scheduling]")
{
    ScheduleData::Builder builder;

    builder.add_machine({.name = "M0"});
    builder.add_machine({.name = "M1"});
    builder.add_machine({.name = "M2"});

    builder.add_job({.name = "J0"});

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
//  RCPSP resource constraints
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleData: RCPSP resources", "[scheduling]")
{
    ScheduleData::Builder builder;

    builder.add_machine();
    builder.add_job();
    builder.add_job();

    int op0 = builder.add_operation(0, {.machine = 0, .duration = 3});
    int op1 = builder.add_operation(1, {.machine = 0, .duration = 2});

    int r0 = builder.add_resource(10);
    int r1 = builder.add_resource(5);

    builder.set_resource_usage(op0, r0, 4);
    builder.set_resource_usage(op0, r1, 2);
    builder.set_resource_usage(op1, r0, 7);

    ScheduleData data = builder.build();

    CHECK(data.num_resources() == 2);
    CHECK(data.resource_capacity(0) == 10);
    CHECK(data.resource_capacity(1) == 5);

    CHECK(data.resource_usage(op0, r0) == 4);
    CHECK(data.resource_usage(op0, r1) == 2);
    CHECK(data.resource_usage(op1, r0) == 7);
    CHECK(data.resource_usage(op1, r1) == 0);  // not set, defaults to 0
}

// ---------------------------------------------------------------------------
//  Extra precedence constraints
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleData: extra precedence constraints", "[scheduling]")
{
    ScheduleData::Builder builder;

    builder.add_machine();
    builder.add_machine();

    builder.add_job();
    builder.add_job();

    // J0: op0, op1 (intra-job: op0 -> op1)
    int op0 = builder.add_operation(0, {.machine = 0, .duration = 2});
    int op1 = builder.add_operation(0, {.machine = 1, .duration = 3});

    // J1: op2 (single operation)
    int op2 = builder.add_operation(1, {.machine = 0, .duration = 1});

    // Extra: op2 must come before op0 (cross-job precedence).
    builder.add_precedence(op2, op0);

    ScheduleData data = builder.build();

    // Should have: op0->op1 (intra-job), op2->op0 (extra).
    auto precs = data.precedences();
    REQUIRE(precs.size() == 2);

    // Intra-job first, then extra.
    CHECK(precs[0].before == 0);  // op0
    CHECK(precs[0].after == 1);   // op1
    CHECK(precs[1].before == 2);  // op2
    CHECK(precs[1].after == 0);   // op0
}

// ---------------------------------------------------------------------------
//  Objective setting
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleData: objective types", "[scheduling]")
{
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

    SECTION("total flow time") {
        builder.set_objective(ScheduleObjective::TotalFlowTime);
        auto data = builder.build();
        CHECK(data.objective() == ScheduleObjective::TotalFlowTime);
    }
}

// ---------------------------------------------------------------------------
//  ScheduleModel::solve() returns a result (stub)
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleModel: solve returns stub result", "[scheduling]")
{
    ScheduleModel model;

    model.add_machine({.name = "M0"});
    model.add_machine({.name = "M1"});

    int j0 = model.add_job({.name = "Job0"});
    model.add_operation(j0, {.machine = 0, .duration = 3});
    model.add_operation(j0, {.machine = 1, .duration = 2});

    int j1 = model.add_job({.name = "Job1"});
    model.add_operation(j1, {.machine = 1, .duration = 4});
    model.add_operation(j1, {.machine = 0, .duration = 1});

    model.minimize_makespan();

    Result result = model.solve(TimeLimit(1.0));

    // Stub: feasible_ is false (no solver yet).
    CHECK_FALSE(result.feasible());

    // Elapsed time should be recorded (> 0).
    CHECK(result.elapsed_seconds() >= 0.0);
}

// ---------------------------------------------------------------------------
//  ScheduleModel: solve with empty model returns default result
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleModel: solve with empty model", "[scheduling]")
{
    ScheduleModel model;
    Result result = model.solve(TimeLimit(0.1));
    CHECK_FALSE(result.feasible());
}

// ---------------------------------------------------------------------------
//  ScheduleModel: warm start
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleModel: set_initial_schedule accepted", "[scheduling]")
{
    ScheduleModel model;

    model.add_machine();
    model.add_machine();

    int j = model.add_job();
    model.add_operation(j, {.machine = 0, .duration = 3});
    model.add_operation(j, {.machine = 1, .duration = 2});

    // Provide initial schedule: op0 on M0 at t=0, op1 on M1 at t=3.
    model.set_initial_schedule({{0, 0}, {1, 3}});

    // Should not throw; solve is still a stub.
    Result result = model.solve(TimeLimit(0.1));
    CHECK_FALSE(result.feasible());
}

// ---------------------------------------------------------------------------
//  ScheduleModel: job metadata (release time, due date, weight)
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleData: job metadata preserved", "[scheduling]")
{
    ScheduleData::Builder builder;

    builder.add_machine();
    builder.add_job({.name = "Urgent", .release_time = 5, .due_date = 20, .weight = 3});
    builder.add_operation(0, {.machine = 0, .duration = 4});

    ScheduleData data = builder.build();

    CHECK(data.job(0).name == "Urgent");
    CHECK(data.job(0).release_time == 5);
    CHECK(data.job(0).due_date == 20);
    CHECK(data.job(0).weight == 3);
}

// ---------------------------------------------------------------------------
//  Error handling: invalid job index
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleModel: invalid job index throws", "[scheduling]")
{
    ScheduleModel model;
    model.add_machine();

    CHECK_THROWS_AS(model.add_operation(0, {.machine = 0, .duration = 1}),
                    std::out_of_range);
    CHECK_THROWS_AS(model.add_operation(-1, {.machine = 0, .duration = 1}),
                    std::out_of_range);
}
