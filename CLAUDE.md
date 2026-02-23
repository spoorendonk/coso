# primal-rsp — Primal Heuristics for Routing, Scheduling & Production Planning

## Git Workflow

- **Never commit directly to master.** Always create a feature branch, push, and open a PR.
- **If user says "commit" while on master**: create a feature branch, commit there, push, and open a PR automatically.
- **Linear history only.** Merge PRs with squash or rebase (no merge commits).
- **No force-push to master.**

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
  cli/                — CLI tool (primal-solve)
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

- `primal::RoutingModel` — Routing model (add_depot, add_client, add_vehicle_type, solve)
- `primal::ScheduleModel` — Scheduling model (add_job, add_operation, add_machine)
- `primal::AssignmentModel` — Assignment model (add_employee, add_shift_type)
- `primal::PackingModel` — Packing model (add_bin_type, add_item)
- `primal::Result` — Solver result (cost, feasible, routes/schedule/assignments, elapsed)
- `primal::TimeLimit` — Solver stop criterion

## Namespace

All code under `primal::` namespace.

## Documentation

- `README.md` — project overview, quick start, build instructions
- `docs/ROADMAP.md` — full design plan: modeling interface, architecture, problem catalog, implementation roadmap, design decisions, references
- `CLAUDE.md` — this file (build/test/workflow instructions for AI assistants)

## Fullgate

When the user says **"fullgate"**, run this sequence in order. Each step can also be invoked individually by name:

1. **Feature branch** — create one if not already on a feature branch
2. **Create PR** — if no PR exists for the current branch
3. **Sync master** — pull latest master and merge into the current feature branch, resolve conflicts
4. **Tests** — check if new/updated tests are needed and add them
5. **Update docs** — update docs/ROADMAP.md, README.md as needed
6. **Push & update PR**
7. **Review** — thoroughly review the PR (code quality, correctness, style, tests, performance)
8. **Build** — `cmake --build build -j$(nproc)`
9. **Test** — `ctest --test-dir build -j$(nproc)`
10. **Push & update PR** again with any fixes
11. **Finalize** — if nothing more to do: squash-merge the PR, delete feature branch (local + remote), pull master, switch to master
