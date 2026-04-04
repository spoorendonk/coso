# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

@.devkit/standards/nanobind.md

## Project

COSO (Combinatorial Structure-aware Optimization) — C++23 engine with Python bindings (nanobind) for combinatorial optimization. Exploits problem structure with domain-specific local search, construction heuristics, and metaheuristics instead of LP/MIP flattening.

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
- **Deterministic work counting** (`src/common/work_units.h`): Cross-machine performance comparison via work units instead of wall time. Used by E2E regression gates.
- **Warm start + pinning**: `set_initial_routes()` and `pin()` on RoutingModel for re-optimization.

### Python Bindings

- `python/bindings.cpp`: nanobind wrappings for types and models.
- `python/coso/__init__.py`: Re-exports. `from coso import RoutingModel, solve_instance`.
- Currently bound: `RoutingModel`, `NetworkModel`, `LotSizingModel` (not all six models yet).

### Testing Structure

- **C++ tests** (`tests/`): Catch2 v3. One executable per module (e.g., `route_test`, `solution_test`, `model_test`). Test executables are defined in `tests/CMakeLists.txt`.
- **Python tests** (`python/tests/`): pytest. `test_routing.py` for routing + shared types, `test_models.py` for other models.
- **E2E tests** (`tests/e2e/`): JSON scenario-driven. `e2e_runner` binary. Quarantine system in `quarantine.csv`. Deterministic perf regression gates.

### Engine Maturity

| Engine | Status |
|--------|--------|
| Routing | **Validated** — ~1.5% gap to Uchoa BKS |
| Network | Functional — exact for unconstrained MCF |
| Packing | Functional — tested on Falkenauer |
| Lot sizing | Functional |
| Scheduling | Experimental — known correctness issues |
| Assignment | Experimental — partial coverage |
