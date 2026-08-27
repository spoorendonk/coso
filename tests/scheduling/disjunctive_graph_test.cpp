#include "scheduling/disjunctive_graph.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <vector>

using namespace coso;

// ========================================================================== //
//  Helper: build a small 3-job x 3-machine JSP instance                      //
//                                                                             //
//  Job 0: (M0, 3) -> (M1, 2) -> (M2, 2)                                     //
//  Job 1: (M0, 2) -> (M2, 1) -> (M1, 4)                                     //
//  Job 2: (M1, 4) -> (M2, 3)                                                 //
// ========================================================================== //

static DisjunctiveGraph make_3x3() {
    DisjunctiveGraph g(3, 3);

    // Job 0
    g.add_operation(0, 0, 3);  // op 0: M0, dur 3
    g.add_operation(0, 1, 2);  // op 1: M1, dur 2
    g.add_operation(0, 2, 2);  // op 2: M2, dur 2

    // Job 1
    g.add_operation(1, 0, 2);  // op 3: M0, dur 2
    g.add_operation(1, 2, 1);  // op 4: M2, dur 1
    g.add_operation(1, 1, 4);  // op 5: M1, dur 4

    // Job 2
    g.add_operation(2, 1, 4);  // op 6: M1, dur 4
    g.add_operation(2, 2, 3);  // op 7: M2, dur 3

    return g;
}

// ========================================================================== //
//  Tests                                                                      //
// ========================================================================== //

TEST_CASE("DisjunctiveGraph basics", "[scheduling][disjunctive]") {
    auto g = make_3x3();

    REQUIRE(g.num_jobs() == 3);
    REQUIRE(g.num_machines() == 3);
    REQUIRE(g.num_operations() == 8);

    // Check job-operation mapping.
    REQUIRE(g.job_operations(0) == std::vector<int>{0, 1, 2});
    REQUIRE(g.job_operations(1) == std::vector<int>{3, 4, 5});
    REQUIRE(g.job_operations(2) == std::vector<int>{6, 7});

    // Check operation data.
    REQUIRE(g.operation(0).job == 0);
    REQUIRE(g.operation(0).machine == 0);
    REQUIRE(g.operation(0).duration == 3);

    REQUIRE(g.operation(6).job == 2);
    REQUIRE(g.operation(6).machine == 1);
    REQUIRE(g.operation(6).duration == 4);
}

TEST_CASE("Topological order respects job precedences", "[scheduling][disjunctive]") {
    auto g = make_3x3();

    // Set arbitrary machine sequences.
    g.set_sequence(0, {0, 3});
    g.set_sequence(1, {6, 1, 5});
    g.set_sequence(2, {4, 2, 7});

    auto topo = g.topological_order();
    REQUIRE(static_cast<int>(topo.size()) == g.num_operations());

    // Build position map.
    std::vector<int> pos(g.num_operations());
    for (int i = 0; i < static_cast<int>(topo.size()); ++i) {
        pos[topo[i]] = i;
    }

    // Job 0: op0 < op1 < op2
    REQUIRE(pos[0] < pos[1]);
    REQUIRE(pos[1] < pos[2]);

    // Job 1: op3 < op4 < op5
    REQUIRE(pos[3] < pos[4]);
    REQUIRE(pos[4] < pos[5]);

    // Job 2: op6 < op7
    REQUIRE(pos[6] < pos[7]);

    // Machine sequences should also hold.
    REQUIRE(pos[0] < pos[3]);  // M0: 0 before 3
    REQUIRE(pos[6] < pos[1]);  // M1: 6 before 1
    REQUIRE(pos[1] < pos[5]);  // M1: 1 before 5
    REQUIRE(pos[4] < pos[2]);  // M2: 4 before 2
    REQUIRE(pos[2] < pos[7]);  // M2: 2 before 7
}

TEST_CASE("Forward pass without machine sequences", "[scheduling][disjunctive]") {
    auto g = make_3x3();

    // Without machine sequences, only job precedence arcs exist.

    // Job 0: durations 3, 2, 2
    REQUIRE(g.start_time(0) == 0);
    REQUIRE(g.completion_time(0) == 3);
    REQUIRE(g.start_time(1) == 3);
    REQUIRE(g.completion_time(1) == 5);
    REQUIRE(g.start_time(2) == 5);
    REQUIRE(g.completion_time(2) == 7);

    // Job 1: durations 2, 1, 4
    REQUIRE(g.start_time(3) == 0);
    REQUIRE(g.completion_time(3) == 2);
    REQUIRE(g.start_time(4) == 2);
    REQUIRE(g.completion_time(4) == 3);
    REQUIRE(g.start_time(5) == 3);
    REQUIRE(g.completion_time(5) == 7);

    // Job 2: durations 4, 3
    REQUIRE(g.start_time(6) == 0);
    REQUIRE(g.completion_time(6) == 4);
    REQUIRE(g.start_time(7) == 4);
    REQUIRE(g.completion_time(7) == 7);

    // Makespan = max(7, 7, 7) = 7 (longest job chain).
    REQUIRE(g.critical_path() == 7);
}

TEST_CASE("Critical path with machine sequences", "[scheduling][disjunctive]") {
    auto g = make_3x3();

    // Set machine sequences:
    //   M0: op0 (J0) -> op3 (J1)
    //   M1: op6 (J2) -> op1 (J0) -> op5 (J1)
    //   M2: op4 (J1) -> op2 (J0) -> op7 (J2)
    g.set_sequence(0, {0, 3});
    g.set_sequence(1, {6, 1, 5});
    g.set_sequence(2, {4, 2, 7});

    // Forward pass (manually computed):
    //
    // op0: start=0, dur=3, finish=3          (M0, J0 first)
    // op3: start=max(3,0)=3, dur=2, fin=5    (M0 after op0; J1 first)
    // op6: start=0, dur=4, fin=4             (M1 first; J2 first)
    // op4: start=max(5,0)=5, dur=1, fin=6    (J1 after op3; M2 first)
    // op1: start=max(4,3)=4, dur=2, fin=6    (M1 after op6; J0 after op0)
    // op2: start=max(6,6)=6, dur=2, fin=8    (M2 after op4; J0 after op1)
    // op5: start=max(6,6)=6, dur=4, fin=10   (M1 after op1; J1 after op4)
    // op7: start=max(8,4)=8, dur=3, fin=11   (M2 after op2; J2 after op6)
    //
    // Makespan = 11

    REQUIRE(g.start_time(0) == 0);
    REQUIRE(g.start_time(3) == 3);
    REQUIRE(g.start_time(6) == 0);
    REQUIRE(g.start_time(4) == 5);
    REQUIRE(g.start_time(1) == 4);
    REQUIRE(g.start_time(2) == 6);
    REQUIRE(g.start_time(5) == 6);
    REQUIRE(g.start_time(7) == 8);

    REQUIRE(g.critical_path() == 11);
}

TEST_CASE("Backward pass and critical path operations", "[scheduling][disjunctive]") {
    auto g = make_3x3();

    g.set_sequence(0, {0, 3});
    g.set_sequence(1, {6, 1, 5});
    g.set_sequence(2, {4, 2, 7});

    int makespan = g.critical_path();
    REQUIRE(makespan == 11);

    // Critical path ops: those where earliest_start == latest_start.
    auto cp_ops = g.critical_path_ops();
    REQUIRE(!cp_ops.empty());

    // All critical path ops should satisfy earliest == latest.
    for (int op : cp_ops) {
        REQUIRE(g.start_time(op) == g.latest_start_time(op));
    }

    // The critical path chain should include op0 -> op3 -> op4 -> op2 -> op7
    // with makespan contribution: 3 + 2 + 1 + 2 + 3 = 11.
    std::set<int> cp_set(cp_ops.begin(), cp_ops.end());
    REQUIRE(cp_set.count(0) == 1);  // op0
    REQUIRE(cp_set.count(3) == 1);  // op3
    REQUIRE(cp_set.count(4) == 1);  // op4
    REQUIRE(cp_set.count(2) == 1);  // op2
    REQUIRE(cp_set.count(7) == 1);  // op7
}

TEST_CASE("Changing machine sequences updates makespan", "[scheduling][disjunctive]") {
    auto g = make_3x3();

    // Sequence 1: as above.
    g.set_sequence(0, {0, 3});
    g.set_sequence(1, {6, 1, 5});
    g.set_sequence(2, {4, 2, 7});
    int ms1 = g.critical_path();

    // Sequence 2: swap M0 order.
    g.set_sequence(0, {3, 0});
    int ms2 = g.critical_path();

    // With op3 first on M0:
    //   op3: start=0, dur=2, fin=2
    //   op0: start=2, dur=3, fin=5
    //   op6: start=0, dur=4, fin=4
    //   op4: start=max(2,0)=2, dur=1, fin=3
    //   op1: start=max(5,4)=5, dur=2, fin=7
    //   op5: start=max(7,3)=7, dur=4, fin=11
    //   op2: start=max(7,3)=7, dur=2, fin=9
    //   op7: start=max(9,4)=9, dur=3, fin=12
    // Makespan = 12
    REQUIRE(ms1 == 11);
    REQUIRE(ms2 == 12);
    REQUIRE(ms1 != ms2);
}

TEST_CASE("Longest path between specific operations", "[scheduling][disjunctive]") {
    auto g = make_3x3();

    g.set_sequence(0, {0, 3});
    g.set_sequence(1, {6, 1, 5});
    g.set_sequence(2, {4, 2, 7});

    // source -> sink = makespan = 11
    REQUIRE(g.longest_path(g.source(), g.sink()) == 11);

    // op0 -> sink: longest path from op0 to end = 11
    REQUIRE(g.longest_path(0, g.sink()) == 11);

    // op0 -> op2: longest path measures sum of durations along the way
    // (each node contributes its own duration as the "edge weight" leaving it).
    // Path 0 -> 3 -> 4 -> 2: dur(0)+dur(3)+dur(4) = 3+2+1 = 6
    // Path 0 -> 1 -> 2: dur(0)+dur(1) = 3+2 = 5
    // longest = 6  (this is the earliest start time of op2, which matches)
    REQUIRE(g.longest_path(0, 2) == 6);
    REQUIRE(g.longest_path(0, 2) == g.start_time(2));

    // Unreachable: op7 -> op0 (wrong direction)
    REQUIRE(g.longest_path(7, 0) == -1);
}

TEST_CASE("set_processing_time updates computation", "[scheduling][disjunctive]") {
    auto g = make_3x3();

    g.set_sequence(0, {0, 3});
    g.set_sequence(1, {6, 1, 5});
    g.set_sequence(2, {4, 2, 7});

    int ms_before = g.critical_path();
    REQUIRE(ms_before == 11);

    // Reduce op7 duration from 3 to 1.
    g.set_processing_time(7, 2, 1);
    int ms_after = g.critical_path();
    // op7 was on critical path, reducing it by 2 changes things.
    // The new chain 0->3->4->2->7: 3+2+1+2+1 = 9
    // But check op5 path: 6(4)->1(2)->5(4) = 10, and op5 start=6, fin=10
    // So makespan = max(9, 10) = 10
    REQUIRE(ms_after == 10);
}

TEST_CASE("Empty jobs are handled gracefully", "[scheduling][disjunctive]") {
    // 2 jobs declared but only 1 has operations.
    DisjunctiveGraph g(2, 1);
    g.add_operation(0, 0, 5);

    REQUIRE(g.num_operations() == 1);
    REQUIRE(g.critical_path() == 5);
    REQUIRE(g.start_time(0) == 0);
    REQUIRE(g.completion_time(0) == 5);
}

TEST_CASE("Single operation", "[scheduling][disjunctive]") {
    DisjunctiveGraph g(1, 1);
    g.add_operation(0, 0, 10);

    REQUIRE(g.critical_path() == 10);
    REQUIRE(g.start_time(0) == 0);
    REQUIRE(g.completion_time(0) == 10);
    REQUIRE(g.latest_start_time(0) == 0);

    auto cp = g.critical_path_ops();
    REQUIRE(cp.size() == 1);
    REQUIRE(cp[0] == 0);
}
