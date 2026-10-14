#include "scheduling/parsers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Taillard JSP parser
// ---------------------------------------------------------------------------

TEST_CASE("parse_taillard_jsp - small 2x3 instance", "[scheduling][parser]") {
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

TEST_CASE("parse_taillard_jsp - error on empty input", "[scheduling][parser]") {
    CHECK_THROWS_AS(parse_taillard_jsp(""), std::runtime_error);
    CHECK_THROWS_AS(parse_taillard_jsp("# just comments\n"), std::runtime_error);
}

// ---------------------------------------------------------------------------
//  FJSP parser
// ---------------------------------------------------------------------------

TEST_CASE("parse_fjsp - small FJSP instance", "[scheduling][parser]") {
    // 2 jobs, 3 machines, 0-based as in SchedulingLab/fjsp-instances (#232).
    // Job 0: 2 operations
    //   Op 0: 2 eligible machines: (m0, d=3) (m2, d=5)
    //   Op 1: 1 eligible machine:  (m1, d=4)  -> fixed m=1, d=4
    // Job 1: 1 operation
    //   Op 0: 3 eligible machines: (m0,d=2) (m1,d=4) (m2,d=1)
    std::string content = R"(
2 3
2  2 0 3 2 5  1 1 4
1  3 0 2 1 4 2 1
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
    CHECK(data.processing_time(0, 0) == 3);        // machine 0
    CHECK(data.processing_time(0, 1) == INT_MAX);  // machine 1 not eligible
    CHECK(data.processing_time(0, 2) == 5);        // machine 2

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

TEST_CASE("parse_fjsp - error on empty input", "[scheduling][parser]") {
    CHECK_THROWS_AS(parse_fjsp(""), std::runtime_error);
}

TEST_CASE("parse_fjsp - the caller states the machine indexing", "[scheduling][parser]") {
    // The same instance in both live conventions (#232): SchedulingLab's
    // 0-based files and the original 1-based Brandimarte/Hurink ones.
    std::string const zero_based = R"(
2 3
2  2 0 3 2 5  1 1 4
1  3 0 2 1 4 2 1
)";
    std::string const one_based = R"(
2 3
2  2 1 3 3 5  1 2 4
1  3 1 2 2 4 3 1
)";

    SECTION("0-based is the default") {
        auto data = parse_fjsp(zero_based);
        CHECK(data.operation(0).eligible_machines == std::vector<int>{0, 2});
        CHECK(data.operation(1).fixed_machine == 1);
    }

    SECTION("OneBased reads the 1-based file as the same instance") {
        auto a = parse_fjsp(zero_based);
        auto b = parse_fjsp(one_based, FjspIndexing::OneBased);
        for (int o = 0; o < 3; ++o) {
            CHECK(a.operation(o).fixed_machine == b.operation(o).fixed_machine);
            CHECK(a.operation(o).eligible_machines == b.operation(o).eligible_machines);
            CHECK(a.operation(o).durations_per_machine == b.operation(o).durations_per_machine);
            CHECK(a.operation(o).duration == b.operation(o).duration);
        }
    }

    SECTION("a machine out of range under the stated indexing is rejected") {
        // Machine 3 does not exist among 0..2; machine 0 does not among 1..3.
        CHECK_THROWS_WITH(parse_fjsp(one_based),
                          Catch::Matchers::ContainsSubstring("machine 3 is outside 0..2"));
        CHECK_THROWS_WITH(parse_fjsp(zero_based, FjspIndexing::OneBased),
                          Catch::Matchers::ContainsSubstring("machine 0 is outside 1..3"));
        // A fixed (single-eligible) operation is checked as well.
        CHECK_THROWS_AS(parse_fjsp("1 2\n1  1 2 7\n"), std::runtime_error);
    }
}
