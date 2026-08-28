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
ctest --test-dir build --output-on-failure -j$(nproc)
```

`pre-push` runs the three blocks above, so they have to work on a plain
checkout. That is why `test` is C++ only: the default build leaves
`COSO_BUILD_PYTHON=OFF`, so there is no `coso` module for the Python tests to
import, and CI gates on `ctest` alone. Run the Python suite explicitly, against
a bindings build.

Run a single C++ test executable:
```bash
./build/tests/route_test                          # run one test binary
./build/tests/route_test "test name substring"    # run specific test case
```

Python tests (needs a bindings build and `pip install -e '.[dev]'`):
```bash
pytest python/tests -q
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
| Scheduling | **Construction-only** (SGS / SPT dispatch / NEH). `ScheduleModel::solve()` validates every candidate with `sol.feasible()` and returns feasible-but-unoptimised schedules. No working local search: the operators in `src/scheduling/schedule_operators.cpp` are not wired into `solve()` and carry the unsound cycle guard of #185. NEH crashes on operations with no feasible machine (#188), and the operators and perturbations can still build cyclic disjunctive graphs (#189) — the tests covering both are `SKIP`-ed, each naming its issue |
| Assignment | Construction + VND; not validated |

## Gates

Three git hooks live in `.githooks/`, tracked in the repo. `cmake -B build` points
`core.hooksPath` at that directory, so **configuring the build is what turns the
gates on** — a fresh clone has them inert until then.

| Hook | Runs | On failure |
|---|---|---|
| `commit-msg` | Conventional Commits format, subject ≤ 72 chars | blocks |
| `pre-commit` | Formats and auto-fixes staged files, re-stages, then runs the tests for the languages that changed | blocks on test failure |
| `pre-push` | Clean build + full suite, then clang-tidy, shellcheck, ruff complexity, mypy | blocks on build/test failure; lint findings warn only |

`pre-push` takes its clean/build/test commands from the fenced blocks in
**## Build & Test** above, so that section is executable configuration, not just
documentation. Renaming a fence or moving the heading makes the hook block and
say which fence it could not find. The `sed` range that reads it ends at the next
`## ` heading, so **## Build & Test** must stay followed by another level-2
heading.

Tools resolve from `.venv/bin` first, then `PATH` (`.githooks/resolve-tools.sh`).
A tool that is missing everywhere is reported, never skipped in silence.

**clang 18 or newer**, so the distro's own package works: Ubuntu 24.04 ships
clang 18, 26.04 ships 21. Everything that can *block* is version-stable, and was
measured rather than assumed — clang-format 18.1.8 through 23.1.0 all leave the
tree byte-identical, and clang-tidy 18 reports nothing for the four checks
`pre-commit` auto-fixes or for the naming rules. `pre-push`'s clang-tidy list is
advisory and currently empty; because the check families are wildcards, a newer
clang-tidy knows more checks and may add to it, which means a longer or shorter
advisory list, never a different verdict.

clang-tidy runs clean today, at 0 findings over all 155 translation units, and
should stay that way — a list people scroll past is worth no more than no check
at all. Getting there needed two things beyond tuning: the vendored dependencies
are fetched `SYSTEM` so their headers are not analysed, and the 39 checks the
codebase does not currently satisfy are switched off in `.clang-tidy` and
tracked in #190 with counts and a re-enable order. The other 166 stay on. When a
new finding appears, fix it or add the check to #190 — do not let the list grow.

There are no Claude Code hooks. Two rules they used to enforce are now
conventions, and still expected:

- **Branch from `main`, never from another feature branch.** Run
  `git checkout main && git pull` first.
- **Use the project venv explicitly** (`.venv/bin/python`, `.venv/bin/pytest`)
  rather than bare `python`/`pip`/`pytest`.

A `PostToolUse` formatter is not possible: the hook cannot see which file was
edited, so it can never format anything.

## Workflow

Trunk-based, linear history on `main`. Commit directly to `main` and push once
the hooks pass. Feature branches are optional, short-lived, and always cut from
`main`; rebase or squash to keep `main` linear.

Commit messages follow Conventional Commits — `type: description` or
`type(scope): description`, subject ≤ 72 chars, explaining **why** rather than
what. Types: `feat`, `fix`, `refactor`, `test`, `docs`, `style`, `perf`, `chore`,
`build`, `ci`.

`/review` runs a multi-agent review of the diff against `main`. It is available
only where `.claude/` exists, which is gitignored — so a fresh clone or worktree
does not have it, and nothing gates on it having run.

When Claude gets something wrong, fix this file in the same commit. That is the
feedback loop.

## Code Standards

### C++

- Target C++23. Use modern features where they earn their place (concepts,
  ranges, `constexpr`). `std::expected` is not used anywhere yet.
- Style is enforced by `.clang-format` (Google base, 100 cols, 4-space indent)
  and `.clang-tidy`. Do not hand-fix formatting — `pre-commit` rewrites it.
- `SortIncludes` and `IncludeBlocks: Regroup` are on, so a reformat reorders
  includes and can expose a header that leaned on a transitive include.
- `#pragma once` for include guards. Minimize includes in headers;
  forward-declare where possible.

Naming, as `.clang-tidy` enforces it:

| Kind | Form | Example |
|---|---|---|
| Types (class/struct/enum) | `CamelCase` | `ProblemData` |
| Functions and methods | `lower_case` | `add_client` |
| Variables and parameters | `lower_case` | `vehicle_type` |
| Private/protected members | `lower_case_` | `num_machines_` |
| Non-public helper methods | `lower_case` or `lower_case_` | `update_` |
| Compile-time constants | `kCamelCase` | `kTickToUnit` |
| Namespaces | `lower_case` | `coso` |

Two deliberate exceptions are encoded in `.clang-tidy`: enumerators that mirror
an external spec or are standard acronyms stay upper-case (`EUC_2D`,
`FULL_MATRIX`, `SPT`), and a bare uppercase letter is OR notation from the
literature (`P` products, `T` periods, `D` dimensions).

### CMake

- `CMAKE_EXPORT_COMPILE_COMMANDS` is on; clang-tidy needs it.
- Dependencies come in via `FetchContent`.
- Sources are listed in the root `CMakeLists.txt`; only `tests/` and `python/`
  have their own.

### Testing (Catch2 v3)

- Test files: `<module>_test.cpp` under `tests/`, one executable per module,
  registered in `tests/CMakeLists.txt`.
- `TEST_CASE("descriptive sentence", "[tag][tag]")`, with `SECTION` for variants.
- A test for known-broken code is `SKIP`-ed with the issue number in the message,
  never deleted and never left failing.

### Python

- Style is enforced by `ruff` (format + lint) and `mypy --strict`, both
  configured in `pyproject.toml`. There are no quiet defaults — `pre-commit`
  passes the flags it needs.
- Full type annotations everywhere; built-in generics (`list[int]`) and `|`
  unions.
- Test files: `test_<module>.py` in `python/tests/`, with shared fixtures in
  `conftest.py`.
- Dependencies pin `>=` lower bounds in `pyproject.toml`.

### nanobind Bindings

- All bindings live in the single file `python/bindings.cpp`; `python/coso/__init__.py`
  re-exports them.
- C++ methods are already `snake_case`, so they bind through unchanged.
- Default to nanobind-managed ownership. Use `nb::rv_policy::reference` only when
  C++ keeps ownership and guarantees the object outlives Python's reference; never
  return a raw pointer without an explicit lifetime annotation.
- Test bindings from Python with pytest, not from C++ — round-trip where possible.

## Working Style

- Don't over-engineer. Three similar lines beat a premature abstraction. Avoid
  abstractions, features, and error handling beyond what the task needs.
- Don't add unrequested features, and don't refactor code adjacent to a fix.
- Comment only where the logic isn't self-evident.
- Performance matters; profile before optimising, and don't trade it away for
  tidiness.
- Read a file before modifying it. Verify an API exists before calling it.
- Say so when you change approach — don't quietly switch strategies.
- Follow an agreed plan; if it should change, stop and discuss rather than
  diverging silently. No TODO placeholders or stubs unless asked.
- When implementing from a paper or reference implementation, match it exactly.
  No early exits, iteration caps, or shortcuts that change behaviour unless
  asked for them.
- Never ship an engine or feature that is known-broken. Disable it so it raises a
  clear error, or delete it — a status line in a doc is not a guard.
