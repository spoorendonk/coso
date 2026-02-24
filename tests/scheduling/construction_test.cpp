#include <catch2/catch_test_macros.hpp>

#include "scheduling/construction.h"
#include "scheduling/schedule_data.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: verify schedule feasibility
// ---------------------------------------------------------------------------

namespace {

/// Check that a schedule respects all precedence and machine constraints.
void check_feasible(ScheduleData const& data, Result const& result)
{
    REQUIRE(result.feasible());
    REQUIRE(static_cast<int>(result.schedule().size()) == data.num_operations());

    // Precedence constraints: before must finish <= after starts.
    for (auto const& arc : data.precedences()) {
        auto const& before = result.schedule()[arc.before];
        auto const& after  = result.schedule()[arc.after];
        int dur_before = data.processing_time(arc.before, before.machine);
        REQUIRE(dur_before < INT_MAX);
        CHECK(before.start_time + dur_before <= after.start_time);
    }

    // Machine constraints: no two operations overlap on the same machine.
    for (int m = 0; m < data.num_machines(); ++m) {
        // Collect operations on this machine.
        struct Interval { int start; int end; int op; };
        std::vector<Interval> intervals;
        for (int o = 0; o < data.num_operations(); ++o) {
            if (result.schedule()[o].machine == m) {
                int dur = data.processing_time(o, m);
                REQUIRE(dur < INT_MAX);
                intervals.push_back({result.schedule()[o].start_time,
                                     result.schedule()[o].start_time + dur,
                                     o});
            }
        }
        // Sort by start time, check no overlaps.
        std::sort(intervals.begin(), intervals.end(),
                  [](auto const& a, auto const& b) {
                      return a.start < b.start;
                  });
        for (int i = 0; i + 1 < static_cast<int>(intervals.size()); ++i) {
            CHECK(intervals[i].end <= intervals[i + 1].start);
        }
    }

    // Resource constraints: at each time step, usage <= capacity.
    if (data.num_resources() > 0) {
        int ms = result.makespan();
        for (int r = 0; r < data.num_resources(); ++r) {
            std::vector<int> usage(ms + 1, 0);
            for (int o = 0; o < data.num_operations(); ++o) {
                int u = data.resource_usage(o, r);
                if (u == 0) continue;
                int m   = result.schedule()[o].machine;
                int dur = data.processing_time(o, m);
                int st  = result.schedule()[o].start_time;
                for (int t = st; t < st + dur; ++t) {
                    usage[t] += u;
                    CHECK(usage[t] <= data.resource_capacity(r));
                }
            }
        }
    }
}

/// Build a small JSP instance (2 jobs x 3 machines, classic example).
///
///  Job 0: M0(3) -> M1(2) -> M2(4)
///  Job 1: M1(2) -> M0(3) -> M2(1)
ScheduleData make_jsp_2x3()
{
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_machine({.name = "M1"});
    b.add_machine({.name = "M2"});

    b.add_job({.name = "J0"});
    b.add_job({.name = "J1"});

    // Job 0
    b.add_operation(0, {.machine = 0, .duration = 3});  // op 0
    b.add_operation(0, {.machine = 1, .duration = 2});  // op 1
    b.add_operation(0, {.machine = 2, .duration = 4});  // op 2

    // Job 1
    b.add_operation(1, {.machine = 1, .duration = 2});  // op 3
    b.add_operation(1, {.machine = 0, .duration = 3});  // op 4
    b.add_operation(1, {.machine = 2, .duration = 1});  // op 5

    return b.build();
}

/// Build a small RCPSP instance.
///
///  4 operations on 4 separate machines, 1 resource with capacity 2.
///  Job 0: op0 (M0, dur=3) -> op1 (M1, dur=3)
///  Job 1: op2 (M2, dur=3) -> op3 (M3, dur=3)
///  Resource usage = 1 per op, capacity = 2.
///  Ops 0 and 2 can run in parallel (different machines, usage 1+1=2 <= cap).
///  Then ops 1 and 3 can run in parallel. Optimal makespan = 6.
ScheduleData make_rcpsp_small()
{
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_machine({.name = "M1"});
    b.add_machine({.name = "M2"});
    b.add_machine({.name = "M3"});

    b.add_job({.name = "J0"});
    b.add_job({.name = "J1"});

    // Job 0: op0 (M0) -> op1 (M1)
    b.add_operation(0, {.machine = 0, .duration = 3});  // op 0
    b.add_operation(0, {.machine = 1, .duration = 3});  // op 1

    // Job 1: op2 (M2) -> op3 (M3)
    b.add_operation(1, {.machine = 2, .duration = 3});  // op 2
    b.add_operation(1, {.machine = 3, .duration = 3});  // op 3

    // Resource with capacity 2.
    int r = b.add_resource(2);
    b.set_resource_usage(0, r, 1);
    b.set_resource_usage(1, r, 1);
    b.set_resource_usage(2, r, 1);
    b.set_resource_usage(3, r, 1);

    return b.build();
}

/// Build a flow-shop instance (3 jobs x 2 machines).
///
///  Each job visits M0 then M1.
///  Job 0: M0(4), M1(3)
///  Job 1: M0(2), M1(5)
///  Job 2: M0(3), M1(2)
ScheduleData make_flowshop_3x2()
{
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_machine({.name = "M1"});

    b.add_job({.name = "J0"});
    b.add_job({.name = "J1"});
    b.add_job({.name = "J2"});

    // Job 0: M0(4), M1(3)
    b.add_operation(0, {.machine = 0, .duration = 4});  // op 0
    b.add_operation(0, {.machine = 1, .duration = 3});  // op 1

    // Job 1: M0(2), M1(5)
    b.add_operation(1, {.machine = 0, .duration = 2});  // op 2
    b.add_operation(1, {.machine = 1, .duration = 5});  // op 3

    // Job 2: M0(3), M1(2)
    b.add_operation(2, {.machine = 0, .duration = 3});  // op 4
    b.add_operation(2, {.machine = 1, .duration = 2});  // op 5

    return b.build();
}

} // namespace

// ---------------------------------------------------------------------------
//  SGS tests
// ---------------------------------------------------------------------------

TEST_CASE("SGS: small JSP instance", "[scheduling][construction]")
{
    auto data = make_jsp_2x3();
    auto result = construct_sgs(data);

    check_feasible(data, result);
    CHECK(result.makespan() > 0);
    // The optimal makespan for this instance is 9.
    CHECK(result.makespan() <= 11);  // SGS should find a reasonable schedule.
}

TEST_CASE("SGS: RCPSP with resource constraints", "[scheduling][construction]")
{
    auto data = make_rcpsp_small();
    auto result = construct_sgs(data);

    check_feasible(data, result);
    // With capacity 2 and usage 1 per op, two ops can run in parallel.
    // Optimal makespan = 6 (two parallel pairs of duration 3).
    CHECK(result.makespan() == 6);
}

// ---------------------------------------------------------------------------
//  NEH tests
// ---------------------------------------------------------------------------

TEST_CASE("NEH: small flow-shop instance", "[scheduling][construction]")
{
    auto data = make_flowshop_3x2();
    auto result = construct_neh(data);

    check_feasible(data, result);
    CHECK(result.makespan() > 0);
    // NEH should produce a good quality schedule.
    // Total lower bound: max(sum on M0, sum on M1) = max(9, 10) = 10.
    // A reasonable schedule should be <= 15.
    CHECK(result.makespan() <= 15);
}

TEST_CASE("NEH: small JSP instance", "[scheduling][construction]")
{
    auto data = make_jsp_2x3();
    auto result = construct_neh(data);

    check_feasible(data, result);
    CHECK(result.makespan() > 0);
}

// ---------------------------------------------------------------------------
//  Dispatching rule tests
// ---------------------------------------------------------------------------

TEST_CASE("Dispatch SPT: small JSP instance", "[scheduling][construction]")
{
    auto data = make_jsp_2x3();
    auto result = construct_dispatch(data, DispatchRule::SPT);

    check_feasible(data, result);
    CHECK(result.makespan() > 0);
}

TEST_CASE("Dispatch LPT: small JSP instance", "[scheduling][construction]")
{
    auto data = make_jsp_2x3();
    auto result = construct_dispatch(data, DispatchRule::LPT);

    check_feasible(data, result);
    CHECK(result.makespan() > 0);
}

TEST_CASE("Dispatch SPT: RCPSP instance (no resource check in dispatch)",
          "[scheduling][construction]")
{
    // Dispatching without resource constraints — uses single machine,
    // so resource constraints are implicitly satisfied by sequencing.
    auto data = make_rcpsp_small();
    auto result = construct_dispatch(data, DispatchRule::SPT);

    // Feasibility check includes precedence and machine overlap checks.
    check_feasible(data, result);
    CHECK(result.makespan() > 0);
}

TEST_CASE("Dispatch: single job, operations ordered correctly",
          "[scheduling][construction]")
{
    ScheduleData::Builder b;
    b.add_machine({.name = "M0"});
    b.add_job({.name = "J0"});

    b.add_operation(0, {.machine = 0, .duration = 5});  // op 0
    b.add_operation(0, {.machine = 0, .duration = 3});  // op 1
    b.add_operation(0, {.machine = 0, .duration = 7});  // op 2

    auto data = b.build();
    auto result = construct_dispatch(data, DispatchRule::SPT);

    check_feasible(data, result);
    // Single job, single machine => sequential: makespan = 5 + 3 + 7 = 15.
    CHECK(result.makespan() == 15);

    // Operations must be in order: op0 then op1 then op2.
    CHECK(result.schedule()[0].start_time == 0);
    CHECK(result.schedule()[1].start_time == 5);
    CHECK(result.schedule()[2].start_time == 8);
}

TEST_CASE("All heuristics produce feasible schedules on same instance",
          "[scheduling][construction]")
{
    auto data = make_jsp_2x3();

    auto r_sgs = construct_sgs(data);
    auto r_neh = construct_neh(data);
    auto r_spt = construct_dispatch(data, DispatchRule::SPT);
    auto r_lpt = construct_dispatch(data, DispatchRule::LPT);

    check_feasible(data, r_sgs);
    check_feasible(data, r_neh);
    check_feasible(data, r_spt);
    check_feasible(data, r_lpt);

    // All should find makespans in a reasonable range.
    CHECK(r_sgs.makespan() >= 9);
    CHECK(r_neh.makespan() >= 9);
    CHECK(r_spt.makespan() >= 9);
    CHECK(r_lpt.makespan() >= 9);
}
