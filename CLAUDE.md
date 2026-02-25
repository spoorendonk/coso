# COSO — Combinatorial Structure-aware Optimization

## Quick Reference

```bash
# build
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

# test
ctest --test-dir build -j$(nproc)
```

Download benchmark instances: `./tests/data/download_benchmarks.sh`
Tests skip automatically when instances are missing.

## Git Workflow

- Never commit directly to `main`. Always feature branches.
- If on main when committing: create branch, commit, push, open PR.
- Linear history (squash-merge or rebase-merge). No force-push to `main`.

## Architecture

```
src/
  model/              — User-facing modeling API (RoutingModel, ScheduleModel, etc.)
  routing/            — CVRP/VRPTW engine (routes, resources, operators, local search)
  scheduling/         — JSP/FJSP/RCPSP engine
  assignment/         — Nurse rostering / timetabling engine
  packing/            — Bin packing engine
  search/             — Shared metaheuristic shells (ILS, HGS, tabu, LA)
  cli/                — CLI tool (coso-solve)
python/               — nanobind Python bindings
tests/                — Catch2 tests and benchmarks per engine
docs/                 — Design plan, benchmarks
```

See `docs/roadmap.md` for full design plan: modeling interface, architecture, problem catalog,
implementation roadmap with work units, design decisions, and references.

## Key Types

- `coso::RoutingModel` — Routing model (add_depot, add_client, add_vehicle_type, solve)
- `coso::ScheduleModel` — Scheduling model (add_job, add_operation, add_machine)
- `coso::AssignmentModel` — Assignment model (add_employee, add_shift_type)
- `coso::PackingModel` — Packing model (add_bin_type, add_item)
- `coso::Result` — Solver result (cost, feasible, routes/schedule/assignments, elapsed)

## Coding Conventions

- C++23, GCC 14, `coso::` namespace
- "Extract don't abstract" — build routing first, then extract shared infrastructure

## Dependencies

GCC 14, C++23, CMake 3.25+, Catch2 + nanobind (FetchContent), optional TBB (`apt install libtbb-dev`)

## Autonomous Agent Workflow

Work autonomously with minimal human interaction.
Progress lives in files and git — not in your context window.

### Grind Loop

1. Understand the task — read relevant code, docs, tests
2. Implement the change
3. Build — if it fails, read errors, fix, rebuild
4. Test — if tests fail, read failures, fix, retest
5. Repeat 2–4 until build and tests pass clean
6. Self-review: correctness, edge cases, performance
7. If review finds issues, go back to 2
8. Commit, push, update the PR

### When to Stop and Ask

Only stop and ask a human when:
- The task has multiple valid interpretations
- A fix requires changing the public API or architecture
- You discover a bug in unrelated code you shouldn't touch

Otherwise: keep going until build and tests pass.

### Claiming Work

- Add label `agent-wip` when you open or start working on an issue or PR
- Check for `agent-wip` before picking up work — never work on labeled items
- Remove `agent-wip` and close/merge when done

### Teams

When a task has independent sub-tasks, launch a team.
Each teammate runs in its own worktree (isolated repo copy).
Lead agent integrates: merge branches, resolve conflicts, run full build/test.
Do NOT use teams for sequential work.

### Fullgate

When the user says **"fullgate"**, run this sequence:

1. **Branch** — create feature branch if needed
2. **PR** — create draft PR if none exists
3. **Sync** — pull latest main, merge into feature branch, resolve conflicts
4. **Tests** — add or update tests as needed
5. **Docs** — update docs/roadmap.md, README.md as needed
6. **Push** — push branch, update PR description
7. **Review** — self-review: correctness, style, tests, performance
8. **Build** — `cmake --build build -j$(nproc)`
9. **Test** — `ctest --test-dir build -j$(nproc)`
10. **Push** — push any review fixes
11. **Finalize** — squash-merge PR, delete branch, pull main
