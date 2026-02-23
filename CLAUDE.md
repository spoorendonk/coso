# COSO — Combinatorial Structure-aware Optimization

## Git Workflow

- **Never commit directly to main.** Always create a feature branch, push, and open a PR.
- **If user says "commit" while on main**: create a feature branch, commit there, push, and open a PR automatically.
- **Linear history only.** Merge PRs with squash or rebase (no merge commits).
- **No force-push to main.**

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Test

```bash
ctest --test-dir build -j$(nproc)
```

Benchmark instances (CVRPLIB, Solomon, Taillard, etc.) are not in git.
Download before running benchmarks:
```bash
./tests/data/download_benchmarks.sh
```
Tests skip automatically when instances are missing.

## Dependencies

- GCC 14, C++23
- CMake 3.25+
- `apt install libtbb-dev` (optional, enables multi-threaded solving)
- Catch2: fetched via CMake FetchContent (v3.7.1)
- nanobind: fetched via CMake FetchContent (Python bindings, optional)

## Architecture

```
src/
  model/              — User-facing modeling API (C++ headers)
    routing_model.h       Routing model
    schedule_model.h      Scheduling model
    assignment_model.h    Assignment model
    packing_model.h       Packing model
    types.h               Shared types (Coord, TimeWindow, CostParams, etc.)
    result.h              Solver result (cost, routes/schedule/assignments)
  routing/            — CVRP/VRPTW engine (internal)
    problem_data.h        Compiled instance data
    solution.h            Route-based solution
    route.h               Route with resource-based evaluation
    resources/            Load, duration, distance, pickup-delivery, etc.
    operators/            Exchange(N,M), SWAP*, ruin-and-recreate
    local_search.h
    cost_evaluator.h
  scheduling/         — JSP/FJSP/RCPSP engine
  assignment/         — Nurse rostering / timetabling engine
  packing/            — Bin packing engine
  search/             — Shared metaheuristic shells (ILS, HGS, tabu, LA)
  cli/                — CLI tool (coso-solve)
python/               — nanobind Python bindings
tests/                — Catch2 tests and benchmarks
  model/                User-facing API tests
  routing/
  scheduling/
  assignment/
  packing/
  search/
  data/                 Benchmark instances (downloaded, not in git)
```

## Key Types

- `coso::RoutingModel` — Routing model (add_depot, add_client, add_vehicle_type, solve)
- `coso::ScheduleModel` — Scheduling model (add_job, add_operation, add_machine)
- `coso::AssignmentModel` — Assignment model (add_employee, add_shift_type)
- `coso::PackingModel` — Packing model (add_bin_type, add_item)
- `coso::Result` — Solver result (cost, feasible, routes/schedule/assignments, elapsed)
- `coso::TimeLimit` — Solver stop criterion

## Namespace

All code under `coso::` namespace.

## Documentation

- `README.md` — project overview, quick start, build instructions
- `docs/ROADMAP.md` — full design plan: modeling interface, architecture, problem catalog, implementation roadmap, design decisions, references
- `CLAUDE.md` — this file (build/test/workflow instructions for AI assistants)

## Agent Coordination

The roadmap (docs/ROADMAP.md section 7) defines work units with IDs like `2.3`,
`5.1`, `8.6`. Each work unit maps to a branch name (e.g., `5.3-precedence-resource`).

**Before suggesting or starting any work unit:**
1. Check open branches: `git branch -a`
2. Check open PRs: `gh pr list`
3. Check for running agents on this machine (background tasks, worktrees)
4. Never start a work unit that another agent has an open branch or PR for
5. Prefer the lowest-numbered unblocked, unclaimed work unit

## Fullgate

When the user says **"fullgate"**, run this sequence in order. Each step can also be invoked individually by name:

1. **Feature branch** — create one if not already on a feature branch
2. **Create PR** — if no PR exists for the current branch
3. **Sync main** — pull latest main and merge into the current feature branch, resolve conflicts
4. **Tests** — check if new/updated tests are needed and add them
5. **Update docs** — update docs/ROADMAP.md, README.md as needed
6. **Push & update PR**
7. **Review** — thoroughly review the PR (code quality, correctness, style, tests, performance)
8. **Build** — `cmake --build build -j$(nproc)`
9. **Test** — `ctest --test-dir build -j$(nproc)`
10. **Push & update PR** again with any fixes
11. **Finalize** — if nothing more to do: squash-merge the PR, delete feature branch (local + remote), pull main, switch to main
