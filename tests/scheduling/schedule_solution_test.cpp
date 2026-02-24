#include <catch2/catch_test_macros.hpp>

#include "scheduling/schedule_data.h"
#include "scheduling/schedule_solution.h"

using namespace coso;

// ========================================================================== //
//  Helper: build a classic 3x3 JSP instance (3 jobs, 3 machines).            //
//                                                                             //
//  Job 0: (M0, 3) -> (M1, 2) -> (M2, 2)                                     //
//  Job 1: (M0, 2) -> (M2, 1) -> (M1, 4)                                     //
//  Job 2: (M1, 4) -> (M2, 3) -> (M0, 1)                                     //
// ========================================================================== //

static ScheduleData make_3x3_jsp()
{
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_machine({.name = "M1"});
    b.add_machine({.name = "M2"});

    b.add_job({.name = "J0"});
    b.add_job({.name = "J1"});
    b.add_job({.name = "J2"});

    // Job 0: ops 0, 1, 2
    b.add_operation(0, {.machine = 0, .duration = 3});
    b.add_operation(0, {.machine = 1, .duration = 2});
    b.add_operation(0, {.machine = 2, .duration = 2});

    // Job 1: ops 3, 4, 5
    b.add_operation(1, {.machine = 0, .duration = 2});
    b.add_operation(1, {.machine = 2, .duration = 1});
    b.add_operation(1, {.machine = 1, .duration = 4});

    // Job 2: ops 6, 7, 8
    b.add_operation(2, {.machine = 1, .duration = 4});
    b.add_operation(2, {.machine = 2, .duration = 3});
    b.add_operation(2, {.machine = 0, .duration = 1});

    return b.build();
}

TEST_CASE("ScheduleSolution: empty solution is infeasible", "[scheduling]")
{
    auto data = make_3x3_jsp();
    ScheduleSolution sol(data);

    CHECK_FALSE(sol.feasible());
    CHECK_FALSE(sol.all_assigned());
    CHECK(sol.num_assigned() == 0);
    CHECK(sol.makespan() == 0);
}

TEST_CASE("ScheduleSolution: assign and unassign", "[scheduling]")
{
    auto data = make_3x3_jsp();
    ScheduleSolution sol(data);

    sol.assign(0, 0, 0);
    CHECK(sol.num_assigned() == 1);
    CHECK(sol.assignment(0).machine == 0);
    CHECK(sol.assignment(0).start_time == 0);
    CHECK(sol.completion_time(0) == 3);

    sol.unassign(0);
    CHECK(sol.num_assigned() == 0);
    CHECK_FALSE(sol.assignment(0).assigned());
    CHECK(sol.completion_time(0) == -1);
}

TEST_CASE("ScheduleSolution: reassign updates correctly", "[scheduling]")
{
    auto data = make_3x3_jsp();
    ScheduleSolution sol(data);

    sol.assign(0, 0, 0);
    CHECK(sol.num_assigned() == 1);

    // Reassign to a different start time — count should stay at 1.
    sol.assign(0, 0, 5);
    CHECK(sol.num_assigned() == 1);
    CHECK(sol.assignment(0).start_time == 5);
    CHECK(sol.completion_time(0) == 8);
}

// ========================================================================== //
//  Build a feasible schedule for the 3x3 JSP and check objectives.            //
//                                                                             //
//  Machine 0:  J0-op0 [0,3)   J1-op3 [3,5)   J2-op8 [10,11)                 //
//  Machine 1:  J2-op6 [0,4)   J0-op1 [4,6)   J1-op5 [6,10)                  //
//  Machine 2:  J1-op4 [5,6)   J0-op2 [6,8)   J2-op7 [6,9)  OVERLAP!        //
//                                                                             //
//  Let's use a valid schedule instead:                                        //
//  Machine 0:  J0-op0 [0,3)   J1-op3 [3,5)   J2-op8 [10,11)                 //
//  Machine 1:  J2-op6 [0,4)   J0-op1 [4,6)   J1-op5 [6,10)                  //
//  Machine 2:  J1-op4 [5,6)   J0-op2 [6,8)   J2-op7 [8,11)                  //
//                                                                             //
//  Check precedences:                                                         //
//    J0: op0 ends 3 <= op1 starts 4, op1 ends 6 <= op2 starts 6. OK          //
//    J1: op3 ends 5 <= op4 starts 5, op4 ends 6 <= op5 starts 6. OK          //
//    J2: op6 ends 4 <= op7 starts 8, op7 ends 11 > op8 starts 10. FAIL!      //
//                                                                             //
//  Fix: op8 starts at 11.                                                     //
//  Machine 0:  J0-op0 [0,3)   J1-op3 [3,5)   J2-op8 [11,12)                 //
//  Machine 1:  J2-op6 [0,4)   J0-op1 [4,6)   J1-op5 [6,10)                  //
//  Machine 2:  J1-op4 [5,6)   J0-op2 [6,8)   J2-op7 [8,11)                  //
//  Makespan = 12                                                              //
// ========================================================================== //

static void build_feasible_schedule(ScheduleSolution& sol)
{
    // Job 0: op0(M0,3), op1(M1,2), op2(M2,2)
    sol.assign(0, 0, 0);    // [0, 3)
    sol.assign(1, 1, 4);    // [4, 6)
    sol.assign(2, 2, 6);    // [6, 8)

    // Job 1: op3(M0,2), op4(M2,1), op5(M1,4)
    sol.assign(3, 0, 3);    // [3, 5)
    sol.assign(4, 2, 5);    // [5, 6)
    sol.assign(5, 1, 6);    // [6, 10)

    // Job 2: op6(M1,4), op7(M2,3), op8(M0,1)
    sol.assign(6, 1, 0);    // [0, 4)
    sol.assign(7, 2, 8);    // [8, 11)
    sol.assign(8, 0, 11);   // [11, 12)
}

TEST_CASE("ScheduleSolution: feasible schedule", "[scheduling]")
{
    auto data = make_3x3_jsp();
    ScheduleSolution sol(data);
    build_feasible_schedule(sol);

    CHECK(sol.feasible());
    CHECK(sol.all_assigned());
    CHECK(sol.no_machine_overlaps());
    CHECK(sol.precedences_respected());
    CHECK(sol.num_assigned() == 9);
}

TEST_CASE("ScheduleSolution: makespan computation", "[scheduling]")
{
    auto data = make_3x3_jsp();
    ScheduleSolution sol(data);
    build_feasible_schedule(sol);

    // Max completion: op8 ends at 12.
    CHECK(sol.makespan() == 12);
}

TEST_CASE("ScheduleSolution: job completion times", "[scheduling]")
{
    auto data = make_3x3_jsp();
    ScheduleSolution sol(data);
    build_feasible_schedule(sol);

    CHECK(sol.job_completion_time(0) == 8);   // op2 ends at 8
    CHECK(sol.job_completion_time(1) == 10);  // op5 ends at 10
    CHECK(sol.job_completion_time(2) == 12);  // op8 ends at 12
}

TEST_CASE("ScheduleSolution: total flow time", "[scheduling]")
{
    auto data = make_3x3_jsp();
    ScheduleSolution sol(data);
    build_feasible_schedule(sol);

    // Flow time = sum of job completion times = 8 + 10 + 12 = 30.
    CHECK(sol.total_flow_time() == 30);
}

TEST_CASE("ScheduleSolution: weighted tardiness", "[scheduling]")
{
    // Build a problem with due dates and weights.
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_machine({.name = "M1"});

    b.add_job({.name = "J0", .due_date = 5, .weight = 2});
    b.add_job({.name = "J1", .due_date = 10, .weight = 3});

    // Job 0: op0(M0, 3) -> op1(M1, 2)
    b.add_operation(0, {.machine = 0, .duration = 3});
    b.add_operation(0, {.machine = 1, .duration = 2});

    // Job 1: op2(M1, 4) -> op3(M0, 1)
    b.add_operation(1, {.machine = 1, .duration = 4});
    b.add_operation(1, {.machine = 0, .duration = 1});

    b.set_objective(ScheduleObjective::TotalWeightedTardiness);
    auto data = b.build();

    ScheduleSolution sol(data);
    // Schedule:
    //   M0: op0 [0,3)  op3 [4,5)
    //   M1: op2 [0,4)  op1 [4,6)
    sol.assign(0, 0, 0);  // [0, 3)
    sol.assign(1, 1, 4);  // [4, 6)
    sol.assign(2, 1, 0);  // [0, 4)
    sol.assign(3, 0, 4);  // [4, 5)

    CHECK(sol.feasible());

    // J0 completes at 6, due 5 => tardiness 1, weighted: 2*1 = 2
    // J1 completes at 5, due 10 => tardiness 0, weighted: 3*0 = 0
    CHECK(sol.job_completion_time(0) == 6);
    CHECK(sol.job_completion_time(1) == 5);
    CHECK(sol.total_weighted_tardiness() == 2);
    CHECK(sol.objective() == 2);
}

TEST_CASE("ScheduleSolution: machine overlap detection", "[scheduling]")
{
    auto data = make_3x3_jsp();
    ScheduleSolution sol(data);

    // Create an overlap on machine 0: two ops running at the same time.
    sol.assign(0, 0, 0);  // [0, 3)
    sol.assign(3, 0, 2);  // [2, 4) — overlaps with op0!

    // Assign remaining ops validly just to have all assigned.
    sol.assign(1, 1, 3);
    sol.assign(2, 2, 5);
    sol.assign(4, 2, 0);
    sol.assign(5, 1, 5);
    sol.assign(6, 1, 0);
    sol.assign(7, 2, 3);
    sol.assign(8, 0, 4);

    CHECK_FALSE(sol.no_machine_overlaps());
    CHECK_FALSE(sol.feasible());
}

TEST_CASE("ScheduleSolution: precedence violation detection", "[scheduling]")
{
    auto data = make_3x3_jsp();
    ScheduleSolution sol(data);

    // Job 0: op0 -> op1 -> op2.
    // Violate: op1 starts before op0 finishes.
    sol.assign(0, 0, 5);   // [5, 8)
    sol.assign(1, 1, 3);   // [3, 5) — starts before op0 finishes at 8!
    sol.assign(2, 2, 10);

    sol.assign(3, 0, 0);
    sol.assign(4, 2, 2);
    sol.assign(5, 1, 5);
    sol.assign(6, 1, 0);
    sol.assign(7, 2, 5);
    sol.assign(8, 0, 9);

    CHECK_FALSE(sol.precedences_respected());
    CHECK_FALSE(sol.feasible());
}

TEST_CASE("ScheduleSolution: machine_operations returns sorted", "[scheduling]")
{
    auto data = make_3x3_jsp();
    ScheduleSolution sol(data);
    build_feasible_schedule(sol);

    // Machine 0: op0 [0,3), op3 [3,5), op8 [11,12)
    auto m0_ops = sol.machine_operations(0);
    REQUIRE(m0_ops.size() == 3);
    CHECK(m0_ops[0] == 0);
    CHECK(m0_ops[1] == 3);
    CHECK(m0_ops[2] == 8);

    // Machine 1: op6 [0,4), op1 [4,6), op5 [6,10)
    auto m1_ops = sol.machine_operations(1);
    REQUIRE(m1_ops.size() == 3);
    CHECK(m1_ops[0] == 6);
    CHECK(m1_ops[1] == 1);
    CHECK(m1_ops[2] == 5);
}

TEST_CASE("ScheduleSolution: FJSP with flexible machine assignment", "[scheduling]")
{
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_machine({.name = "M1"});

    b.add_job({.name = "J0"});

    // Single flexible operation: can run on M0 (dur 5) or M1 (dur 3).
    b.add_operation(0, {
        .eligible_machines = {0, 1},
        .durations_per_machine = {5, 3},
    });

    auto data = b.build();
    ScheduleSolution sol(data);

    // Assign to M1 (faster).
    sol.assign(0, 1, 0);
    CHECK(sol.feasible());
    CHECK(sol.completion_time(0) == 3);
    CHECK(sol.makespan() == 3);

    // Reassign to M0 (slower).
    sol.assign(0, 0, 0);
    CHECK(sol.feasible());
    CHECK(sol.completion_time(0) == 5);
    CHECK(sol.makespan() == 5);
}

TEST_CASE("ScheduleSolution: objective dispatch", "[scheduling]")
{
    auto data = make_3x3_jsp();
    // Default objective is Makespan.
    ScheduleSolution sol(data);
    build_feasible_schedule(sol);

    CHECK(sol.objective() == sol.makespan());
    CHECK(sol.objective() == 12);
}
