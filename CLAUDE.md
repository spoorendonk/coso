# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

COSO (Combinatorial Structure-aware Optimization) — two products built together:

1. **The Model API** — declare-then-solve, one typed model per problem class. Backend-flexible by design: the same declaration is meant to be solvable by COSO's own engine or by a plugged-in backend (PyVRP, OR-Tools CP-SAT, HiGHS, user-supplied). The plugin protocol is **not built yet** — see #176.
2. **The COSO engine** — C++23 with Python bindings (nanobind). Exploits problem structure with domain-specific local search, construction heuristics, and metaheuristics instead of LP/MIP flattening.

Direction and open work live in GitHub issues; the charter is #173. There is no roadmap file.

## Build & Test

```clean
rm -rf build
```

```build
cmake -B build && cmake --build build -j$(nproc)
```

```test
ctest --test-dir build --output-on-failure -j$(nproc) && pytest --tb=short -q
```

Run a single C++ test executable:
```bash
./build/tests/route_test                          # run one test binary
./build/tests/route_test "test name substring"    # run specific test case
```

Run a single Python test:
```bash
pytest python/tests/test_routing.py -q
pytest python/tests/test_routing.py::test_name -q
```

Build with Python bindings:
```bash
cmake -B build -DCOSO_BUILD_PYTHON=ON && cmake --build build -j$(nproc)
```

Build with TBB (parallel solving):
```bash
cmake -B build -DCOSO_USE_TBB=ON && cmake --build build -j$(nproc)
```

## Architecture

### Layered Design

```
Model APIs (public) → Engine (domain-specific) → Search (generic metaheuristics)
```

1. **Model layer** (`src/model/`): Six typed APIs — `RoutingModel`, `NetworkModel`, `LotSizingModel`, `ScheduleModel`, `AssignmentModel`, `PackingModel`. Each compiles user input into an immutable engine-specific data structure. Public headers are in `src/model/*.h`.

2. **Engine layer** (`src/routing/`, `src/scheduling/`, `src/assignment/`, `src/packing/`, `src/network/`, `src/lotsizing/`): Domain-specific solvers. Each engine has its own compiled data representation, solution type, construction heuristic, and local search operators.

3. **Search layer** (`src/search/`): Problem-agnostic metaheuristics — ILS, GA/HGS, GLS, portfolio solver. These wrap engine-specific operators and solutions. Portfolio runs ILS (fast convergence) then GA (deeper exploration).

### Routing Engine (most mature)

The routing engine is the reference architecture for other engines:

- **ProblemData** (`src/routing/problem_data.h`): Immutable compiled instance. Struct-of-arrays layout for cache efficiency. Precomputed granular neighbor lists (k-NN). Node numbering: depots `0..n_d-1`, clients `n_d..n_d+n_c-1`.
- **Resources** (`src/routing/resources/`): Pluggable constraint modules (load, duration, distance, breaks, depot, precedence, sync, compartment, skill, type incompatibility). Constraints are resources attached to routes, not embedded in the solution.
- **Solution/Route** (`src/routing/solution.h`, `route.h`): Solution = all routes + unassigned clients. Route = single vehicle's client sequence.
- **Local search** (`src/routing/local_search.h`): First-improvement descent over granular neighborhoods.
- **Operators** (`src/routing/operators/`): Exchange, swap-star, route-split, insert-optional, pair operators, relocate-with-depot.

### Key Design Patterns

- **Compiled instance**: Models compile to immutable `*Data` structs (e.g., `ProblemData`, `ScheduleData`). This enables caching and efficient repeated solving.
- **Resource-based constraints**: Constraints are pluggable resource objects, not hardcoded into solutions.
- **Deterministic work counting** (`src/common/work_units.h`): Cross-machine performance comparison via work units instead of wall time. The `deterministic_work` E2E check uses it; there is no perf-regression gate tooling in the repo.
- **Warm start + pinning**: `set_initial_routes()` and `pin()` on RoutingModel for re-optimization.

### Python Bindings

- `python/bindings.cpp`: nanobind wrappings for types and models.
- `python/coso/__init__.py`: Re-exports. `from coso import RoutingModel, solve_instance`.
- Currently bound: `RoutingModel`, `NetworkModel`, `LotSizingModel` (not all six models yet).

### Testing Structure

- **C++ tests** (`tests/`): Catch2 v3. One executable per module (e.g., `route_test`, `solution_test`, `model_test`). Test executables are defined in `tests/CMakeLists.txt`.
- **Python tests** (`python/tests/`): pytest. `test_routing.py` for routing + shared types, `test_models.py` for other models.
- **E2E tests**: the `e2e_smoke` ctest target runs `tests/e2e/run_pack.sh` over `examples/e2e/scenarios/smoke/*.json` — **six scenarios, one per model type**. `e2e_runner` builds a hardcoded toy instance per model type (`examples/e2e/e2e_runner.cpp`, `solve_once`); scenario JSON only supplies id, time limit, and which checks to assert. This is a smoke gate, not variant or benchmark coverage — per-variant instances land in the M1–M6 milestones.
  - `COSO_E2E_APPLY_QUARANTINE=1` makes `run_pack.sh` skip scenario ids listed in `tests/e2e/quarantine.csv`.
- **Benchmark tests**: `benchmark_test`, `vrptw_benchmark_test`, `scheduling_benchmark_test`, `assignment_benchmark_test`, `packing_benchmark_test` (label `benchmark`). Instances come from `tests/data/download_benchmarks.sh`. **No results are published anywhere until a verified run exists** — see #177.

### Engine Maturity

No numbers here until a verified benchmark run backs them (#177).

| Engine | Status |
|--------|--------|
| Routing | Most mature; validated against standard CVRP instances |
| Network | Target scope is multi-commodity flow + network design (#184) — neither implemented. The existing single-commodity min-cost flow solver is not a COSO target: that problem is solved |
| Packing | Functional — FFD + move/swap local search (1-D, vector, conflicts) |
| Lot sizing | Functional — fix-and-optimize bridge |
| Scheduling | **Construction-only** (SGS / SPT dispatch / NEH). `ScheduleModel::solve()` validates every candidate with `sol.feasible()` and returns feasible-but-unoptimised schedules. No working local search: the operators in `src/scheduling/schedule_operators.cpp` are not wired into `solve()` and carry the unsound cycle guard of #185 |
| Assignment | Construction + VND; not validated |
