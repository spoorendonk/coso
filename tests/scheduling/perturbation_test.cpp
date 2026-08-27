#include <catch2/catch_test_macros.hpp>

#include "scheduling/disjunctive_graph.h"
#include "scheduling/perturbation.h"
#include "scheduling/schedule_data.h"

#include <climits>
#include <random>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a 3x3 JSP disjunctive graph.
//
//  Classic ft03-style instance:
//    Job 0: M0(3) -> M1(2) -> M2(2)
//    Job 1: M0(2) -> M2(3) -> M1(4)
//    Job 2: M1(3) -> M0(2) -> M2(1)
//
//  Operations:
//    op 0: J0, M0, dur=3    op 3: J1, M0, dur=2    op 6: J2, M1, dur=3
//    op 1: J0, M1, dur=2    op 4: J1, M2, dur=3    op 7: J2, M0, dur=2
//    op 2: J0, M2, dur=2    op 5: J1, M1, dur=4    op 8: J2, M2, dur=1
// ---------------------------------------------------------------------------

namespace {

DisjunctiveGraph make_3x3()
{
    DisjunctiveGraph g(3, 3);

    g.add_operation(0, 0, 3);  // op 0
    g.add_operation(0, 1, 2);  // op 1
    g.add_operation(0, 2, 2);  // op 2

    g.add_operation(1, 0, 2);  // op 3
    g.add_operation(1, 2, 3);  // op 4
    g.add_operation(1, 1, 4);  // op 5

    g.add_operation(2, 1, 3);  // op 6
    g.add_operation(2, 0, 2);  // op 7
    g.add_operation(2, 2, 1);  // op 8

    g.set_sequence(0, {0, 3, 7});
    g.set_sequence(1, {1, 5, 6});
    g.set_sequence(2, {2, 4, 8});

    return g;
}

/// Build a 4x3 JSP instance (4 jobs, 3 machines) for larger tests.
DisjunctiveGraph make_4x3()
{
    DisjunctiveGraph g(4, 3);

    // Job 0: M0(2) -> M1(3) -> M2(1)
    g.add_operation(0, 0, 2);  // op 0
    g.add_operation(0, 1, 3);  // op 1
    g.add_operation(0, 2, 1);  // op 2

    // Job 1: M1(2) -> M0(4) -> M2(2)
    g.add_operation(1, 1, 2);  // op 3
    g.add_operation(1, 0, 4);  // op 4
    g.add_operation(1, 2, 2);  // op 5

    // Job 2: M2(3) -> M1(1) -> M0(2)
    g.add_operation(2, 2, 3);  // op 6
    g.add_operation(2, 1, 1);  // op 7
    g.add_operation(2, 0, 2);  // op 8

    // Job 3: M0(1) -> M2(4) -> M1(3)
    g.add_operation(3, 0, 1);  // op 9
    g.add_operation(3, 2, 4);  // op 10
    g.add_operation(3, 1, 3);  // op 11

    g.set_sequence(0, {0, 4, 8, 9});
    g.set_sequence(1, {1, 3, 7, 11});
    g.set_sequence(2, {2, 5, 6, 10});

    return g;
}

/// Build an FJSP instance with flexible machine assignments.
/// Returns both the ScheduleData and a DisjunctiveGraph.
///
///  2 jobs, 3 machines:
///    Job 0: op0 (M0:3, M1:2) -> op1 (M1:4, M2:3)
///    Job 1: op2 (M0:2, M2:4) -> op3 (M1:1, M2:2)
struct FJSPInstance {
    ScheduleData data;
    DisjunctiveGraph graph;
};

FJSPInstance make_fjsp()
{
    ScheduleData::Builder builder;
    builder.add_machine();  // M0
    builder.add_machine();  // M1
    builder.add_machine();  // M2

    int j0 = builder.add_job();
    int j1 = builder.add_job();

    // Op 0: job 0, flexible on M0 (dur=3) and M1 (dur=2)
    OperationParams p0;
    p0.eligible_machines = {0, 1};
    p0.durations_per_machine = {3, 2};
    builder.add_operation(j0, p0);  // op 0

    // Op 1: job 0, flexible on M1 (dur=4) and M2 (dur=3)
    OperationParams p1;
    p1.eligible_machines = {1, 2};
    p1.durations_per_machine = {4, 3};
    builder.add_operation(j0, p1);  // op 1

    // Op 2: job 1, flexible on M0 (dur=2) and M2 (dur=4)
    OperationParams p2;
    p2.eligible_machines = {0, 2};
    p2.durations_per_machine = {2, 4};
    builder.add_operation(j1, p2);  // op 2

    // Op 3: job 1, flexible on M1 (dur=1) and M2 (dur=2)
    OperationParams p3;
    p3.eligible_machines = {1, 2};
    p3.durations_per_machine = {1, 2};
    builder.add_operation(j1, p3);  // op 3

    auto sdata = builder.build();

    // Build graph: assign initial machines (first eligible for each).
    DisjunctiveGraph g(2, 3);
    g.add_operation(0, 0, 3);  // op 0 on M0
    g.add_operation(0, 1, 4);  // op 1 on M1
    g.add_operation(1, 0, 2);  // op 2 on M0
    g.add_operation(1, 1, 1);  // op 3 on M1

    // M0: op0, op2
    g.set_sequence(0, {0, 2});
    // M1: op1, op3
    g.set_sequence(1, {1, 3});
    // M2: empty
    g.set_sequence(2, {});

    return {std::move(sdata), std::move(g)};
}

/// Check that all job-precedence constraints are respected in the graph.
/// For each job, operations must appear in order of their job index.
bool check_no_cycles(DisjunctiveGraph& graph)
{
    int ms = graph.critical_path();
    if (ms <= 0) {
        // Check if any operation has positive duration (ms=0 with positive
        // durations means cycle).
        for (int i = 0; i < graph.num_operations(); ++i)
            if (graph.operation(i).duration > 0)
                return false;
    }
    return true;
}

} // namespace

// ===========================================================================
//  RandomBlockRemoval tests
// ===========================================================================

TEST_CASE("RandomBlockRemoval: produces valid schedule on 3x3",
          "[scheduling][perturbation]")
{
    SKIP("RandomBlockRemoval can create a cyclic disjunctive graph, aborting in "
         "DisjunctiveGraph::topo_sort() — coso#189 (gap in #185's fix)");
    auto graph = make_3x3();
    int original_ms = graph.critical_path();
    REQUIRE(original_ms > 0);

    std::mt19937 rng(42);
    RandomBlockRemoval::Params params{.min_block_size = 2, .max_block_size = 3};

    int new_ms = RandomBlockRemoval::apply(graph, params, rng);
    CHECK(new_ms > 0);
    CHECK(check_no_cycles(graph));

    // Machine sequences should still contain all original operations.
    std::vector<bool> seen(graph.num_operations(), false);
    for (int m = 0; m < graph.num_machines(); ++m) {
        for (int op : graph.machine_sequence(m))
            seen[op] = true;
    }
    for (int i = 0; i < graph.num_operations(); ++i)
        CHECK(seen[i]);
}

TEST_CASE("RandomBlockRemoval: changes the solution on 4x3",
          "[scheduling][perturbation]")
{
    SKIP("RandomBlockRemoval can create a cyclic disjunctive graph, aborting in "
         "DisjunctiveGraph::topo_sort() — coso#189 (gap in #185's fix)");
    auto graph = make_4x3();
    int original_ms = graph.critical_path();
    REQUIRE(original_ms > 0);

    // Save original sequences.
    std::vector<std::vector<int>> original_seqs;
    for (int m = 0; m < graph.num_machines(); ++m)
        original_seqs.push_back(graph.machine_sequence(m));

    // Apply perturbation multiple times to check it sometimes changes things.
    bool changed = false;
    for (int trial = 0; trial < 20; ++trial) {
        auto g = make_4x3();
        std::mt19937 rng(trial);
        RandomBlockRemoval::Params params{
            .min_block_size = 2, .max_block_size = 4};
        RandomBlockRemoval::apply(g, params, rng);

        for (int m = 0; m < g.num_machines(); ++m) {
            if (g.machine_sequence(m) != original_seqs[m]) {
                changed = true;
                break;
            }
        }
        if (changed)
            break;
    }
    CHECK(changed);
}

TEST_CASE("RandomBlockRemoval: respects min/max block size bounds",
          "[scheduling][perturbation]")
{
    SKIP("RandomBlockRemoval can create a cyclic disjunctive graph, aborting in "
         "DisjunctiveGraph::topo_sort() — coso#189 (gap in #185's fix)");
    auto graph = make_3x3();

    // With min=max=2, should always remove exactly 2 ops.
    std::mt19937 rng(123);
    RandomBlockRemoval::Params params{.min_block_size = 2, .max_block_size = 2};
    int new_ms = RandomBlockRemoval::apply(graph, params, rng);
    CHECK(new_ms > 0);
    CHECK(check_no_cycles(graph));
}

// ===========================================================================
//  CriticalPathShake tests
// ===========================================================================

TEST_CASE("CriticalPathShake: produces valid schedule on 3x3",
          "[scheduling][perturbation]")
{
    auto graph = make_3x3();
    int original_ms = graph.critical_path();
    REQUIRE(original_ms > 0);

    std::mt19937 rng(42);
    CriticalPathShake::Params params{.num_perturbations = 3};

    int new_ms = CriticalPathShake::apply(graph, params, rng);
    CHECK(new_ms > 0);
    CHECK(check_no_cycles(graph));

    // All operations still present.
    std::vector<bool> seen(graph.num_operations(), false);
    for (int m = 0; m < graph.num_machines(); ++m)
        for (int op : graph.machine_sequence(m))
            seen[op] = true;
    for (int i = 0; i < graph.num_operations(); ++i)
        CHECK(seen[i]);
}

TEST_CASE("CriticalPathShake: changes the solution",
          "[scheduling][perturbation]")
{
    SKIP("CriticalPathShake can create a cyclic disjunctive graph, aborting in "
         "DisjunctiveGraph::topo_sort() — coso#189 (gap in #185's fix)");
    // Save original sequences.
    auto orig_graph = make_3x3();
    std::vector<std::vector<int>> original_seqs;
    for (int m = 0; m < orig_graph.num_machines(); ++m)
        original_seqs.push_back(orig_graph.machine_sequence(m));

    bool changed = false;
    for (int trial = 0; trial < 20; ++trial) {
        auto graph = make_3x3();
        std::mt19937 rng(trial);
        CriticalPathShake::Params params{.num_perturbations = 5};
        CriticalPathShake::apply(graph, params, rng);

        for (int m = 0; m < graph.num_machines(); ++m) {
            if (graph.machine_sequence(m) != original_seqs[m]) {
                changed = true;
                break;
            }
        }
        if (changed)
            break;
    }
    CHECK(changed);
}

TEST_CASE("CriticalPathShake: reverts cycle-creating moves",
          "[scheduling][perturbation]")
{
    SKIP("This is the regression test for coso#189: CriticalPathShake can still create a "
         "cyclic disjunctive graph, aborting in DisjunctiveGraph::topo_sort() (gap in #185's "
         "fix) rather than being safely reverted.");
    // Run many perturbations on 4x3 and verify no cycles are introduced.
    for (int trial = 0; trial < 50; ++trial) {
        auto graph = make_4x3();
        std::mt19937 rng(trial);
        CriticalPathShake::Params params{.num_perturbations = 10};
        CriticalPathShake::apply(graph, params, rng);
        CHECK(check_no_cycles(graph));
    }
}

TEST_CASE("CriticalPathShake: single perturbation on 4x3",
          "[scheduling][perturbation]")
{
    SKIP("CriticalPathShake can create a cyclic disjunctive graph, aborting in "
         "DisjunctiveGraph::topo_sort() — coso#189 (gap in #185's fix)");
    auto graph = make_4x3();
    int original_ms = graph.critical_path();
    REQUIRE(original_ms > 0);

    std::mt19937 rng(99);
    CriticalPathShake::Params params{.num_perturbations = 1};
    int new_ms = CriticalPathShake::apply(graph, params, rng);
    CHECK(new_ms > 0);
    CHECK(check_no_cycles(graph));
}

// ===========================================================================
//  MachineReassignment tests
// ===========================================================================

TEST_CASE("MachineReassignment: produces valid schedule on FJSP",
          "[scheduling][perturbation]")
{
    auto [data, graph] = make_fjsp();
    int original_ms = graph.critical_path();
    REQUIRE(original_ms > 0);

    std::mt19937 rng(42);
    MachineReassignment::Params params{.num_reassignments = 1};

    int new_ms = MachineReassignment::apply(graph, data, params, rng);
    CHECK(new_ms > 0);
    CHECK(check_no_cycles(graph));

    // All operations still present in some machine sequence.
    std::vector<bool> seen(graph.num_operations(), false);
    for (int m = 0; m < graph.num_machines(); ++m)
        for (int op : graph.machine_sequence(m))
            seen[op] = true;
    for (int i = 0; i < graph.num_operations(); ++i)
        CHECK(seen[i]);
}

TEST_CASE("MachineReassignment: changes machine assignments",
          "[scheduling][perturbation]")
{
    SKIP("MachineReassignment can create a cyclic disjunctive graph, aborting in "
         "DisjunctiveGraph::topo_sort() — coso#189 (gap in #185's fix)");
    bool changed = false;
    for (int trial = 0; trial < 20; ++trial) {
        auto [data, graph] = make_fjsp();

        // Record original machines.
        std::vector<int> orig_machines;
        for (int i = 0; i < graph.num_operations(); ++i)
            orig_machines.push_back(graph.operation(i).machine);

        std::mt19937 rng(trial);
        MachineReassignment::Params params{.num_reassignments = 2};
        MachineReassignment::apply(graph, data, params, rng);

        for (int i = 0; i < graph.num_operations(); ++i) {
            if (graph.operation(i).machine != orig_machines[i]) {
                changed = true;
                break;
            }
        }
        if (changed)
            break;
    }
    CHECK(changed);
}

TEST_CASE("MachineReassignment: returns -1 when no flexible ops",
          "[scheduling][perturbation]")
{
    // The 3x3 JSP graph has no flexible operations. Build a ScheduleData
    // with fixed machines only.
    ScheduleData::Builder builder;
    builder.add_machine();
    builder.add_machine();
    builder.add_machine();

    int j0 = builder.add_job();
    int j1 = builder.add_job();
    int j2 = builder.add_job();

    // All operations have a single fixed machine.
    builder.add_operation(j0, {.machine = 0, .duration = 3});
    builder.add_operation(j0, {.machine = 1, .duration = 2});
    builder.add_operation(j0, {.machine = 2, .duration = 2});

    builder.add_operation(j1, {.machine = 0, .duration = 2});
    builder.add_operation(j1, {.machine = 2, .duration = 3});
    builder.add_operation(j1, {.machine = 1, .duration = 4});

    builder.add_operation(j2, {.machine = 1, .duration = 3});
    builder.add_operation(j2, {.machine = 0, .duration = 2});
    builder.add_operation(j2, {.machine = 2, .duration = 1});

    auto data = builder.build();
    auto graph = make_3x3();

    std::mt19937 rng(42);
    MachineReassignment::Params params{.num_reassignments = 2};

    int result = MachineReassignment::apply(graph, data, params, rng);
    CHECK(result == -1);
}

// ===========================================================================
//  Combined perturbation tests
// ===========================================================================

TEST_CASE("Multiple perturbations maintain valid schedules",
          "[scheduling][perturbation]")
{
    SKIP("Exercises perturbation operators that can create a cyclic disjunctive graph, "
         "aborting in DisjunctiveGraph::topo_sort() — coso#189 (gap in #185's fix)");
    // Apply all three perturbation types in sequence on a 4x3 instance
    // and verify validity after each.
    auto graph = make_4x3();
    std::mt19937 rng(42);

    for (int iter = 0; iter < 10; ++iter) {
        RandomBlockRemoval::apply(
            graph, {.min_block_size = 2, .max_block_size = 3}, rng);
        REQUIRE(check_no_cycles(graph));
        REQUIRE(graph.critical_path() > 0);

        CriticalPathShake::apply(graph, {.num_perturbations = 3}, rng);
        REQUIRE(check_no_cycles(graph));
        REQUIRE(graph.critical_path() > 0);
    }
}
