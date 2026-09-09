#include "model/schedule_model.h"

#include "scheduling/parsers.h"
#include "scheduling/schedule_data.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
#include <vector>

using namespace coso;

namespace {

// ---------------------------------------------------------------------------
//  Round-trip mechanism (#202 ruling 2)
// ---------------------------------------------------------------------------
//
//  `ScheduleModel` compiles its declaration into a `ScheduleData` inside
//  `solve()` and never hands it back, and `ScheduleData` has no `operator==`.
//  The two helpers below therefore duplicate, on purpose, (a) the Builder
//  sequence of `src/model/schedule_model.cpp` `solve()` and (b) an equality
//  operator that does not exist.  Both are known duplication and both die with
//  the backend dispatch of #176; until then they can drift from `solve()`, so a
//  change to the compile order there belongs in `compile_declared` too.

/// Replay a declaration into a `ScheduleData`, in the order `solve()` does:
/// machines, jobs, operations, objective.
ScheduleData compile_declared(ScheduleModel const& m) {
    ScheduleData::Builder builder;

    for (int i = 0; i < m.num_machines(); ++i) {
        builder.add_machine(m.machine(i));
    }
    for (int j = 0; j < m.num_jobs(); ++j) {
        builder.add_job(m.job(j));
    }
    for (int o = 0; o < m.num_operations(); ++o) {
        auto const& entry = m.operation(o);
        builder.add_operation(entry.job, entry.params);
    }
    builder.set_objective(m.objective());

    return builder.build();
}

/// Compare every field `ScheduleData` exposes.  Setup times are swept over
/// every (from, to, machine) triple; calendars are compared only through
/// `has_calendar()`, since nothing can populate either today — no
/// `ScheduleModel` setter, and neither parser reaches
/// `Builder::set_setup_time` or `add_machine_available`.  Both sides are
/// therefore always empty, and the sweep pins that they stay equal if one of
/// them ever is not.
void check_same_schedule_data(ScheduleData const& a, ScheduleData const& b) {
    REQUIRE(a.num_machines() == b.num_machines());
    REQUIRE(a.num_jobs() == b.num_jobs());
    REQUIRE(a.num_operations() == b.num_operations());

    CHECK(a.objective() == b.objective());
    CHECK(a.has_setup_times() == b.has_setup_times());
    CHECK(a.has_calendar() == b.has_calendar());

    for (int j = 0; j < a.num_jobs(); ++j) {
        INFO("job " << j);
        CHECK(a.job(j).due_date == b.job(j).due_date);
        CHECK(a.job(j).weight == b.job(j).weight);
        CHECK(a.job(j).operations == b.job(j).operations);
    }

    for (int o = 0; o < a.num_operations(); ++o) {
        INFO("operation " << o);
        CHECK(a.operation(o).job == b.operation(o).job);
        CHECK(a.operation(o).fixed_machine == b.operation(o).fixed_machine);
        CHECK(a.operation(o).duration == b.operation(o).duration);
        CHECK(a.operation(o).eligible_machines == b.operation(o).eligible_machines);
        CHECK(a.operation(o).durations_per_machine == b.operation(o).durations_per_machine);
    }

    auto arcs_a = a.precedences();
    auto arcs_b = b.precedences();
    REQUIRE(arcs_a.size() == arcs_b.size());
    for (size_t i = 0; i < arcs_a.size(); ++i) {
        INFO("precedence arc " << i);
        CHECK(arcs_a[i].before == arcs_b[i].before);
        CHECK(arcs_a[i].after == arcs_b[i].after);
    }

    for (int o = 0; o < a.num_operations(); ++o) {
        for (int m = 0; m < a.num_machines(); ++m) {
            INFO("processing_time(" << o << ", " << m << ")");
            CHECK(a.processing_time(o, m) == b.processing_time(o, m));
        }
    }

    // setup_time() reads 0 everywhere when no matrix is set, so this sweep is a
    // real comparison on both sides whether or not one exists.
    for (int from = 0; from < a.num_operations(); ++from) {
        for (int to = 0; to < a.num_operations(); ++to) {
            for (int m = 0; m < a.num_machines(); ++m) {
                CHECK(a.setup_time(from, to, m) == b.setup_time(from, to, m));
            }
        }
    }
}

/// The budget every solve below runs under.  A single job is a chain, so all
/// three constructions finish immediately; the clock is only a safety net.
TimeLimit budget() {
    return TimeLimit(1.0);
}

}  // namespace

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

// ---------------------------------------------------------------------------
//  Round-trip: Taillard JSP (#202 v1 scope ruling — JSPLIB)
// ---------------------------------------------------------------------------

TEST_CASE("A parsed Taillard JSP instance is declarable field for field",
          "[scheduling][roundtrip]") {
    // The literal of tests/scheduling/parser_test.cpp:15, copied rather than
    // shared: a header would couple the parser's own coverage to this audit's,
    // and either file's literal must be able to fail on its own.
    std::string const content = R"(
# comment line
2 3
0 3  1 2  2 4
1 5  2 1  0 6
)";

    // Step 1 — the parser reads the literal as written.  A mis-typed literal
    // must fail here, not silently weaken the comparison below.
    ScheduleData parsed = parse_taillard_jsp(content);

    REQUIRE(parsed.num_jobs() == 2);
    REQUIRE(parsed.num_machines() == 3);
    REQUIRE(parsed.num_operations() == 6);
    CHECK(parsed.operation(0).fixed_machine == 0);  // job 0, first pair "0 3"
    CHECK(parsed.operation(0).duration == 3);
    CHECK(parsed.operation(3).fixed_machine == 1);  // job 1, first pair "1 5"
    CHECK(parsed.operation(3).duration == 5);
    CHECK(parsed.operation(5).duration == 6);
    CHECK(parsed.precedences().size() == 4);
    CHECK(parsed.objective() == ScheduleObjective::Makespan);

    // Step 2 — the same instance declared through ScheduleModel.
    ScheduleModel model;
    for (int m = 0; m < 3; ++m) {
        model.add_machine();
    }

    int j0 = model.add_job();
    model.add_operation(j0, {.machine = 0, .duration = 3});
    model.add_operation(j0, {.machine = 1, .duration = 2});
    model.add_operation(j0, {.machine = 2, .duration = 4});

    int j1 = model.add_job();
    model.add_operation(j1, {.machine = 1, .duration = 5});
    model.add_operation(j1, {.machine = 2, .duration = 1});
    model.add_operation(j1, {.machine = 0, .duration = 6});

    model.minimize_makespan();

    // Step 3 — exact equality.  Every Builder call parse_taillard_jsp makes has
    // a ScheduleModel setter, so nothing in the JSP format is undeclarable.
    check_same_schedule_data(parsed, compile_declared(model));
}

// ---------------------------------------------------------------------------
//  Round-trip: FJSP (#202 v1 scope ruling — SchedulingLab/fjsp-instances)
// ---------------------------------------------------------------------------

TEST_CASE("A parsed FJSP instance is declarable field for field", "[scheduling][roundtrip]") {
    // The literal of tests/scheduling/parser_test.cpp:75, copied for the same
    // reason as above.  Operation 0 is the flexible one the FJSP format exists
    // for: two eligible machines with *different* durations (3 and 5).
    //
    // The literal is written 1-based because that is what parse_fjsp reads
    // today — it does `machine - 1` at src/scheduling/parsers.cpp:161,174.  The
    // only verified FJSP set (SchedulingLab/fjsp-instances) is 0-based, so the
    // same file taken from that set compiles to machine -1; that is coso#232,
    // and it is deliberately not fixed here.  When it is, this case fails and
    // says so.
    std::string const content = R"(
2 3
2  2 1 3 3 5  1 2 4
1  3 1 2 2 4 3 1
)";

    // Step 1 — the parser reads the literal as written.
    ScheduleData parsed = parse_fjsp(content);

    REQUIRE(parsed.num_jobs() == 2);
    REQUIRE(parsed.num_machines() == 3);
    REQUIRE(parsed.num_operations() == 3);
    CHECK(parsed.operation(0).fixed_machine == -1);  // flexible
    CHECK(parsed.operation(0).eligible_machines == std::vector<int>{0, 2});
    CHECK(parsed.operation(0).durations_per_machine == std::vector<int>{3, 5});
    CHECK(parsed.operation(1).fixed_machine == 1);  // "1 2 4": one machine -> fixed
    CHECK(parsed.operation(1).duration == 4);
    CHECK(parsed.precedences().size() == 1);
    CHECK(parsed.objective() == ScheduleObjective::Makespan);

    // Step 2 — the same instance declared through ScheduleModel.  The machine
    // indices here are 0-based, i.e. post-conversion: the model API has no
    // 1-based dialect, and that asymmetry is exactly what #232 is about.
    ScheduleModel model;
    for (int m = 0; m < 3; ++m) {
        model.add_machine();
    }

    int j0 = model.add_job();
    model.add_operation(j0, {
                                .eligible_machines = {0, 2},
                                .durations_per_machine = {3, 5},
                            });
    model.add_operation(j0, {.machine = 1, .duration = 4});

    int j1 = model.add_job();
    model.add_operation(j1, {
                                .eligible_machines = {0, 1, 2},
                                .durations_per_machine = {2, 4, 1},
                            });

    model.minimize_makespan();

    // Step 3 — exact equality, flexible operations included.
    check_same_schedule_data(parsed, compile_declared(model));
}

// ---------------------------------------------------------------------------
//  Round-trip: single machine, weighted tardiness (OR-Library wt40 / wt50)
// ---------------------------------------------------------------------------

TEST_CASE("An OR-Library wt instance is declarable end to end", "[scheduling][roundtrip]") {
    // OR-Library wt40/wt50 is one of the four sets the v1 ruling verified, and
    // it is what keeps JobParams::due_date, JobParams::weight and
    // TotalWeightedTardiness in the schema.  There is no wt parser in the repo,
    // so this is a declaration test, not a parser round-trip: the three blocks
    // of a wt instance — processing times, weights, due dates — are declared
    // directly and every one of them is then read back off the compiled data.
    std::vector<int> const processing = {6, 4, 9, 3};
    std::vector<int> const weight = {2, 1, 4, 3};
    std::vector<int> const due_date = {10, 5, 22, 7};

    ScheduleModel model;
    int const machine = model.add_machine();
    CHECK(machine == 0);

    for (size_t j = 0; j < processing.size(); ++j) {
        int job = model.add_job({.due_date = due_date[j], .weight = weight[j]});
        model.add_operation(job, {.machine = machine, .duration = processing[j]});
    }
    model.set_objective(ScheduleObjective::TotalWeightedTardiness);

    ScheduleData data = compile_declared(model);

    REQUIRE(data.num_machines() == 1);
    REQUIRE(data.num_jobs() == static_cast<int>(processing.size()));
    REQUIRE(data.num_operations() == static_cast<int>(processing.size()));
    CHECK(data.objective() == ScheduleObjective::TotalWeightedTardiness);

    // 1||sum w_j T_j is one operation per job on the single machine, and no
    // precedence at all — a job of one operation contributes no intra-job arc.
    CHECK(data.precedences().empty());

    for (int j = 0; j < data.num_jobs(); ++j) {
        INFO("job " << j);
        auto const& jd = data.job(j);
        CHECK(jd.due_date == due_date[static_cast<size_t>(j)]);
        CHECK(jd.weight == weight[static_cast<size_t>(j)]);
        REQUIRE(jd.operations.size() == 1);

        int op = jd.operations[0];
        CHECK(op == j);
        CHECK(data.operation(op).job == j);
        CHECK(data.operation(op).fixed_machine == 0);
        CHECK(data.operation(op).duration == processing[static_cast<size_t>(j)]);
        CHECK(data.processing_time(op, 0) == processing[static_cast<size_t>(j)]);
    }
}

// ---------------------------------------------------------------------------
//  Gap pinning: format fields the parsers discard
// ---------------------------------------------------------------------------
//
//  A discarded field never reaches a ScheduleData, so field-by-field equality
//  cannot see it and prose cannot fail.  Each section parses two literals that
//  differ only in the discarded field and asserts they compile identically —
//  an assertion that starts failing the day the field becomes representable.

TEST_CASE("Discarded scheduling format fields stay discarded", "[scheduling][roundtrip][pinning]") {
    SECTION("parse_fjsp drops the header's third value (avg machines per operation)") {
        // src/scheduling/parsers.cpp:118 extracts only num_jobs and
        // num_machines from the header line; :121 records that the optional
        // third value is ignored.  In the Brandimarte/Hurink convention the
        // verified SchedulingLab set uses, it is the average number of
        // machines per operation (mk01's header is `10 6 2`).  The format
        // carries it, the schema has nowhere to put it, and it is derivable
        // from the job lines anyway.
        std::string const without_avg = R"(
2 3
2  2 1 3 3 5  1 2 4
1  3 1 2 2 4 3 1
)";
        std::string const with_avg = R"(
2 3 2
2  2 1 3 3 5  1 2 4
1  3 1 2 2 4 3 1
)";
        std::string const with_wrong_avg = R"(
2 3 99
2  2 1 3 3 5  1 2 4
1  3 1 2 2 4 3 1
)";

        check_same_schedule_data(parse_fjsp(without_avg), parse_fjsp(with_avg));
        check_same_schedule_data(parse_fjsp(with_avg), parse_fjsp(with_wrong_avg));
    }

    SECTION("parse_taillard_jsp drops '#' comment lines, where JSPLIB names the instance") {
        // next_data_line skips them at src/scheduling/parsers.cpp:22.  JSPLIB
        // files carry the instance name, its size and its published bounds in
        // exactly those lines, and the v1 ruling cut MachineParams::name and
        // JobParams::name, so nothing in the schema can hold any of it.  The
        // same helper serves parse_fjsp, so the pin covers both parsers.
        std::string const bare = R"(
2 3
0 3  1 2  2 4
1 5  2 1  0 6
)";
        std::string const annotated = R"(
# instance la01
# Lawrence 2x3; optimum 42
2 3
0 3  1 2  2 4
1 5  2 1  0 6
)";

        check_same_schedule_data(parse_taillard_jsp(bare), parse_taillard_jsp(annotated));
    }
}

// ---------------------------------------------------------------------------
//  The single-job envelope (#202 correction 8)
// ---------------------------------------------------------------------------
//
//  construct_neh() aborts on any instance with two or more jobs (#188), so a
//  single job is everything solve() can evidence.  A single job is a chain:
//  no two operations ever compete for a machine, so nothing disjunctive is
//  observable here.  What *is* observable is that each declared field reaches
//  the returned schedule and the reported cost — asserted by mutation, not by
//  a control value alone.

TEST_CASE("A single-job solve evidences the declared shop fields", "[scheduling][envelope]") {
    SECTION("OperationParams::machine and ::duration reach the returned schedule") {
        auto solve_with = [](int second_duration) {
            ScheduleModel model;
            model.add_machine();
            model.add_machine();
            int j = model.add_job();
            model.add_operation(j, {.machine = 0, .duration = 4});
            model.add_operation(j, {.machine = 1, .duration = second_duration});
            model.minimize_makespan();
            return model.solve(budget());
        };

        Result base = solve_with(6);
        REQUIRE(base.feasible());
        REQUIRE(base.schedule().size() == 2);
        CHECK(base.schedule()[0].machine == 0);  // the declared machines
        CHECK(base.schedule()[1].machine == 1);
        CHECK(base.schedule()[0].start_time == 0);
        CHECK(base.schedule()[1].start_time == 4);  // intra-job chain
        CHECK(base.makespan() == 10);
        CHECK(base.cost() == 10.0);

        // Mutation: a longer second operation is a longer chain.
        Result longer = solve_with(9);
        REQUIRE(longer.feasible());
        CHECK(longer.makespan() == 13);
        CHECK(longer.cost() > base.cost());
    }

    SECTION("JobParams::due_date and ::weight price TotalWeightedTardiness") {
        auto solve_with = [](int due_date, int weight) {
            ScheduleModel model;
            model.add_machine();
            model.add_machine();
            int j = model.add_job({.due_date = due_date, .weight = weight});
            model.add_operation(j, {.machine = 0, .duration = 4});
            model.add_operation(j, {.machine = 1, .duration = 6});
            model.set_objective(ScheduleObjective::TotalWeightedTardiness);
            return model.solve(budget());
        };

        // The chain completes at 10 whatever the due date, so the cost is
        // weight * max(0, 10 - due_date) and nothing else.
        Result late = solve_with(2, 3);
        REQUIRE(late.feasible());
        CHECK(late.makespan() == 10);
        CHECK(late.cost() == 24.0);

        // Mutation on the due date: a job that is not late costs nothing.
        Result on_time = solve_with(100, 3);
        REQUIRE(on_time.feasible());
        CHECK(on_time.makespan() == 10);
        CHECK(on_time.cost() == 0.0);

        // Mutation on the weight: same tardiness, third of the weight.
        Result lighter = solve_with(2, 1);
        REQUIRE(lighter.feasible());
        CHECK(lighter.cost() == 8.0);
        CHECK(lighter.cost() * 3.0 == late.cost());
    }

    SECTION("eligible_machines and durations_per_machine choose the machine") {
        auto solve_with = [](std::vector<int> const& durations) {
            ScheduleModel model;
            for (int m = 0; m < 3; ++m) {
                model.add_machine();
            }
            int j = model.add_job();
            model.add_operation(j, {
                                       .eligible_machines = {0, 2},
                                       .durations_per_machine = durations,
                                   });
            model.minimize_makespan();
            return model.solve(budget());
        };

        Result cheap_on_2 = solve_with({9, 3});
        REQUIRE(cheap_on_2.feasible());
        REQUIRE(cheap_on_2.schedule().size() == 1);
        CHECK(cheap_on_2.schedule()[0].machine == 2);
        CHECK(cheap_on_2.makespan() == 3);

        // Mutation: swap the two declared durations and the chosen machine
        // swaps with them.  Machine 1 is never chosen — it is not eligible.
        Result cheap_on_0 = solve_with({3, 9});
        REQUIRE(cheap_on_0.feasible());
        CHECK(cheap_on_0.schedule()[0].machine == 0);
        CHECK(cheap_on_0.makespan() == 3);
    }
}
