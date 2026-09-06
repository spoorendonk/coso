# COSO — Combinatorial Structure-aware Optimization

COSO is two products, built together:

1. **The Model API.** You declare the problem — depots, clients, vehicles, jobs, machines, bins,
   products — and ask for a solution. One typed model per problem class, no LP/MIP flattening in the
   user's hands. The API is **backend-flexible by design**: the same declaration is meant to be
   solvable by COSO's own engine or by a plugged-in backend (PyVRP, OR-Tools CP-SAT, HiGHS, or one
   you supply). *The plugin protocol is not built yet* — see [#176](../../issues/176).
2. **The COSO engine.** A C++23 solver that exploits problem structure directly, with
   domain-specific local search, construction heuristics, and metaheuristics. It is the default
   where there is evidence it is the right choice, and a product in its own right.

> **Status:** early development. The charter and all open work live in GitHub issues — start at
> [#173](../../issues/173). `Model::solve()` is expected to change as the plugin protocol lands, so
> treat the snippets below as current, not stable.

## Quick Start

```cpp
#include <coso/routing_model.h>

coso::RoutingModel m;
auto depot = m.add_depot(456, 320);
auto vtype = m.add_vehicle_type(4, {.capacity = {15}});
m.add_client(228, 0, {.demand = {1}});
m.add_client(912, 0, {.demand = {1}});
m.add_client(0,   80, {.demand = {3}});

auto result = m.solve(coso::TimeLimit(60));
```

```python
import coso

m = coso.RoutingModel()
depot = m.add_depot(456, 320)

vt = coso.VehicleTypeParams()
vt.capacity = [15]
vtype = m.add_vehicle_type(4, vt)

for x, y, d in [(228, 0, 1), (912, 0, 1), (0, 80, 3)]:
    cp = coso.ClientParams()
    cp.demand = [d]
    m.add_client(x, y, cp)

result = m.solve(coso.TimeLimit(60))
```

Or from a CVRPLIB file:

```cpp
auto result = coso::solve("X-n101-k25.vrp", coso::TimeLimit(60));
```

## Models

| Model | Problems |
|---|---|
| `RoutingModel` | CVRP, multi-dimensional CVRP, VRP with simultaneous pickup and delivery, TSP |
| `ScheduleModel` | JSP, FJSP, RCPSP, flow shop, open shop |
| `AssignmentModel` | Nurse rostering, employee scheduling, multi-activity scheduling |
| `PackingModel` | Bin packing, vector bin packing, bin packing with conflicts |
| `NetworkModel` | Min-cost flow (single commodity) |
| `LotSizingModel` | CLSP (capacitated lot sizing) |

Python bindings currently cover `RoutingModel`, `NetworkModel`, and `LotSizingModel`.

What each model can actually declare, and what each engine does with it, is specified in
[`docs/models.md`](docs/models.md).

```cpp
coso::NetworkModel m;
int s = m.add_node(5, "source");
int t = m.add_node(-5, "sink");
m.add_arc(s, t, /*cost=*/2, /*lower=*/0, /*upper=*/5);
auto r = m.solve(coso::TimeLimit(10));
```

## Engine status

Deliberately unquantified: no gap, percentage, or instance count appears here until a verified
benchmark run backs it. That work is [#177](../../issues/177) and the per-model milestones.

| Engine | Status |
|---|---|
| **Routing** | Most mature, and narrower than the schema. What it enforces is demand against an N-dimensional vehicle capacity, minimising distance; it is validated against standard CVRP instances. Time windows are penalised but never enforced, so a violating solution comes back feasible ([#194](../../issues/194)); `Result::cost` is total distance whatever objective was declared ([#198](../../issues/198)); and 16 of the 27 declarable fields plus four methods — multi-depot, multi-trip, pickup-delivery pairs, optional clients, groups, skills, release times, overtime — are accepted and dropped ([#196](../../issues/196)). Per-field verdicts are in [`docs/models.md`](docs/models.md). |
| **Packing** | Functional — FFD construction with move/swap local search. |
| **Lot sizing** | Single-level CLSP only. Constructions (lot-for-lot, Silver-Meal, part-period balancing) plus a shift/merge/split descent — there is no fix-and-optimize anywhere in the tree. A declared bill of materials is accepted and never read ([#210](../../issues/210)), and an instance whose only feasible plans build ahead of a capacity spike comes back infeasible ([#211](../../issues/211)). |
| **Network** | Target scope is **multi-commodity flow and network design** ([#184](../../issues/184)) — neither is implemented. What exists is a single-commodity min-cost flow solver, which is not a COSO target: that problem is solved. |
| **Scheduling** | **Construction-only** (SGS / SPT dispatch / NEH). `ScheduleModel::solve()` validates every candidate and returns feasible-but-unoptimised schedules. There is no working local search: the disjunctive-graph operators are not wired into `solve()` and carry the unsound cycle guard of [#185](../../issues/185). |
| **Assignment** | Construction + VND. Not validated. |

## Tests and coverage

`ctest -L e2e-smoke` runs six end-to-end scenarios — **one per model type**, each a small toy
instance — plus three that assert the runner rejects a scenario whose determinism check is not
bounded by work ([#209](../../issues/209)). It is a smoke gate: it proves the model → engine →
`Result` path runs and is deterministic. It is not variant coverage and not a benchmark.
Per-variant instances arrive with the per-model milestones (M1–M6 in [#173](../../issues/173)).

Benchmark executables (`benchmark_test`, `vrptw_benchmark_test`, `scheduling_benchmark_test`,
`assignment_benchmark_test`, `packing_benchmark_test`, label `benchmark`) run against instances
fetched by `tests/data/download_benchmarks.sh`. Their results are not published until they are
reproducible under [#177](../../issues/177).

## Canonical examples

One runnable C++ example per model family in `examples/canonical/`:
`routing_example.cpp`, `network_example.cpp`, `lotsizing_example.cpp`, `schedule_example.cpp`,
`assignment_example.cpp`, `packing_example.cpp`.

## Build

Requires C++23 and CMake 3.25+. TBB is optional (enables multi-threaded solving).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Test

```bash
ctest --test-dir build -j$(nproc)
```

## Python install

COSO is **not on PyPI yet** ([#217](../../issues/217)) — build from source for now:

```bash
cmake -B build -DCOSO_BUILD_PYTHON=ON && cmake --build build -j$(nproc)
```

Once published, the distribution will be `pycoso`:

```bash
pip install pycoso
```

**`pip install coso` does not install this project** — that name belongs to an unrelated package on
PyPI. The distribution is `pycoso`; the import package is still `coso`:

```python
import coso
```

## License

MIT — see [LICENSE](LICENSE).
