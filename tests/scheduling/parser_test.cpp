#include <catch2/catch_test_macros.hpp>

#include "scheduling/parsers.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Taillard JSP parser
// ---------------------------------------------------------------------------

TEST_CASE("parse_taillard_jsp - small 2x3 instance", "[scheduling][parser]")
{
    // 2 jobs, 3 machines.
    // Job 0: op(m=0,d=3), op(m=1,d=2), op(m=2,d=4)
    // Job 1: op(m=1,d=5), op(m=2,d=1), op(m=0,d=6)
    std::string content = R"(
# comment line
2 3
0 3  1 2  2 4
1 5  2 1  0 6
)";

    auto data = parse_taillard_jsp(content);

    REQUIRE(data.num_jobs() == 2);
    REQUIRE(data.num_machines() == 3);
    REQUIRE(data.num_operations() == 6);

    // Job 0 has operations 0, 1, 2.
    REQUIRE(data.job(0).operations.size() == 3);
    CHECK(data.job(0).operations[0] == 0);
    CHECK(data.job(0).operations[1] == 1);
    CHECK(data.job(0).operations[2] == 2);

    // Job 1 has operations 3, 4, 5.
    REQUIRE(data.job(1).operations.size() == 3);
    CHECK(data.job(1).operations[0] == 3);

    // Check operation 0: machine 0, duration 3.
    CHECK(data.operation(0).fixed_machine == 0);
    CHECK(data.operation(0).duration == 3);
    CHECK(data.processing_time(0, 0) == 3);

    // Check operation 3 (job 1, first op): machine 1, duration 5.
    CHECK(data.operation(3).fixed_machine == 1);
    CHECK(data.operation(3).duration == 5);
    CHECK(data.processing_time(3, 1) == 5);

    // Check operation 5 (job 1, last op): machine 0, duration 6.
    CHECK(data.operation(5).fixed_machine == 0);
    CHECK(data.operation(5).duration == 6);

    // Precedence: 4 intra-job arcs (0->1, 1->2 for job 0; 3->4, 4->5 for job 1).
    auto prec = data.precedences();
    CHECK(prec.size() == 4);

    CHECK(data.objective() == ScheduleObjective::Makespan);
}

TEST_CASE("parse_taillard_jsp - error on empty input", "[scheduling][parser]")
{
    CHECK_THROWS_AS(parse_taillard_jsp(""), std::runtime_error);
    CHECK_THROWS_AS(parse_taillard_jsp("# just comments\n"), std::runtime_error);
}

// ---------------------------------------------------------------------------
//  PSPLIB RCPSP parser
// ---------------------------------------------------------------------------

TEST_CASE("parse_psplib - small RCPSP instance", "[scheduling][parser]")
{
    // Minimal PSPLIB .sm file with 4 "jobs" (activities):
    //   1 = source (dummy), 4 = sink (dummy)
    //   2 and 3 are real activities.
    //   2 renewable resources with capacities 4 and 3.
    //   Precedence: 1 -> 2, 1 -> 3, 2 -> 4, 3 -> 4.
    std::string content = R"(
************************************************************************
file with calculation calculation calculation
************************************************************************
                     jobs (incl. supersource/sink ):          4
                     - renewable                 :  2   R1 R2
                     - nonrenewable               :  0
                     - doubly constrained          :  0
************************************************************************
PRECEDENCE RELATIONS:
jobnr.    #modes  #successors   successors
   1        1          2           2  3
   2        1          1           4
   3        1          1           4
   4        1          0
************************************************************************
REQUESTS/DURATIONS:
jobnr. mode duration  R 1  R 2
------------------------------------------------------------------------
  1      1     0       0    0
  2      1     5       2    1
  3      1     3       1    2
  4      1     0       0    0
************************************************************************
RESOURCEAVAILABILITIES:
  R 1  R 2
    4    3
************************************************************************
)";

    auto data = parse_psplib(content);

    // 4 PSPLIB activities -> 4 jobs, each with 1 operation.
    REQUIRE(data.num_jobs() == 4);
    REQUIRE(data.num_operations() == 4);
    REQUIRE(data.num_resources() == 2);

    // Resource capacities.
    CHECK(data.resource_capacity(0) == 4);
    CHECK(data.resource_capacity(1) == 3);

    // Activity 2 (index 1 in 0-based jobs): duration 5, uses R1=2, R2=1.
    CHECK(data.operation(1).duration == 5);
    CHECK(data.resource_usage(1, 0) == 2);
    CHECK(data.resource_usage(1, 1) == 1);

    // Activity 3 (index 2): duration 3, uses R1=1, R2=2.
    CHECK(data.operation(2).duration == 3);
    CHECK(data.resource_usage(2, 0) == 1);
    CHECK(data.resource_usage(2, 1) == 2);

    // Dummy source (activity 1, op 0): duration 0.
    CHECK(data.operation(0).duration == 0);

    // Dummy sink (activity 4, op 3): duration 0.
    CHECK(data.operation(3).duration == 0);

    // Precedence arcs: 1->2, 1->3, 2->4, 3->4 (plus 0 intra-job since
    // each job has exactly 1 operation). So 4 arcs total.
    auto prec = data.precedences();
    CHECK(prec.size() == 4);

    CHECK(data.objective() == ScheduleObjective::Makespan);
}

// ---------------------------------------------------------------------------
//  FJSP parser
// ---------------------------------------------------------------------------

TEST_CASE("parse_fjsp - small FJSP instance", "[scheduling][parser]")
{
    // 2 jobs, 3 machines.
    // Job 0: 2 operations
    //   Op 0: 2 eligible machines: (m1, d=3) (m3, d=5)  -> 0-based: (0,3) (2,5)
    //   Op 1: 1 eligible machine:  (m2, d=4)             -> 0-based: fixed m=1, d=4
    // Job 1: 1 operation
    //   Op 0: 3 eligible machines: (m1,d=2) (m2,d=4) (m3,d=1)
    std::string content = R"(
2 3
2  2 1 3 3 5  1 2 4
1  3 1 2 2 4 3 1
)";

    auto data = parse_fjsp(content);

    REQUIRE(data.num_jobs() == 2);
    REQUIRE(data.num_machines() == 3);
    REQUIRE(data.num_operations() == 3);

    // Job 0 has 2 operations (indices 0, 1).
    REQUIRE(data.job(0).operations.size() == 2);

    // Op 0: flexible, eligible on machines 0 and 2.
    auto const& op0 = data.operation(0);
    CHECK(op0.fixed_machine == -1);
    REQUIRE(op0.eligible_machines.size() == 2);
    CHECK(op0.eligible_machines[0] == 0);
    CHECK(op0.eligible_machines[1] == 2);
    CHECK(op0.durations_per_machine[0] == 3);
    CHECK(op0.durations_per_machine[1] == 5);

    // Processing time matrix checks.
    CHECK(data.processing_time(0, 0) == 3);   // machine 0
    CHECK(data.processing_time(0, 1) == INT_MAX); // machine 1 not eligible
    CHECK(data.processing_time(0, 2) == 5);   // machine 2

    // Op 1: fixed on machine 1, duration 4.
    auto const& op1 = data.operation(1);
    CHECK(op1.fixed_machine == 1);
    CHECK(op1.duration == 4);
    CHECK(data.processing_time(1, 1) == 4);

    // Job 1 has 1 operation (index 2).
    REQUIRE(data.job(1).operations.size() == 1);

    // Op 2: flexible, eligible on all 3 machines.
    auto const& op2 = data.operation(2);
    CHECK(op2.fixed_machine == -1);
    REQUIRE(op2.eligible_machines.size() == 3);
    CHECK(data.processing_time(2, 0) == 2);
    CHECK(data.processing_time(2, 1) == 4);
    CHECK(data.processing_time(2, 2) == 1);

    // Precedence: job 0 has 2 ops -> 1 arc (0->1). Job 1 has 1 op -> 0 arcs.
    auto prec = data.precedences();
    CHECK(prec.size() == 1);
    CHECK(prec[0].before == 0);
    CHECK(prec[0].after == 1);

    CHECK(data.objective() == ScheduleObjective::Makespan);
}

TEST_CASE("parse_fjsp - error on empty input", "[scheduling][parser]")
{
    CHECK_THROWS_AS(parse_fjsp(""), std::runtime_error);
}
