# COSO — Combinatorial Structure-aware Optimization

Declarative modeling + LP-free solving for combinatorial optimization.
C++ engine with Python bindings (nanobind).

Many combinatorial problems — routing, scheduling, packing — have rich structure
that generic MIP solvers destroy when they flatten everything into rows and columns.
COSO exploits that structure directly with problem-specific local search, construction
heuristics, and metaheuristics.

You pick the problem class (e.g. `RoutingModel`), declare the instance, and the
solver selects the algorithm portfolio: which construction heuristic, which local
search operators, which metaheuristic wrapper.

> **Status:** Early development. The routing engine is validated against standard
> benchmarks. Other engines are work-in-progress.

## Quick Start

```cpp
#include <coso/routing_model.h>

coso::RoutingModel m;
auto depot = m.add_depot(456, 320);
auto vtype = m.add_vehicle_type(4, {.capacity = 15});
m.add_client(228, 0, {.demand = 1});
m.add_client(912, 0, {.demand = 1});
m.add_client(0,   80, {.demand = 3});

auto result = m.solve(coso::TimeLimit(60));
```

```python
import coso

m = coso.RoutingModel()
depot = m.add_depot(456, 320)
vtype = m.add_vehicle_type(4, capacity=15)
m.add_client(228, 0, demand=1)
m.add_client(912, 0, demand=1)
m.add_client(0, 80, demand=3)

result = m.solve(coso.TimeLimit(60))
```

Or from a CVRPLIB file:

```cpp
auto result = coso::solve("X-n101-k25.vrp", coso::TimeLimit(60));
```

## Engines

| Engine | Problems | Approach | Status |
|--------|----------|----------|--------|
| **Routing** | CVRP, VRPTW, PDPTW, TRSP, fleet, multi-trip, ... | Resources + ILS/HGS | **Validated** — tested against Uchoa CVRP instances, ~1.5% gap to BKS |
| **Network** | MCF, RCMCF, liner shipping | Single-commodity SSP (exact) + network local search | **Functional** — exact solver for unconstrained MCF |
| **Packing** | Bin packing, vector bin packing | FFD + local search | **Functional** — tested against Falkenauer instances |
| **Lot sizing** | CLSP, MLCLSP | Constructive + lot-sizing operators | **Functional** — construction heuristics + local improvement |
| **Scheduling** | JSP, FJSP, RCPSP, flow shop, open shop, ... | Disjunctive graph + local search | **Experimental** — known correctness issues under investigation |
| **Assignment** | Nurse rostering, timetabling, employee scheduling | VND + CP filter | **Experimental** — partial metaheuristic coverage |

## Public Model APIs

All engines are available through typed model APIs:

- `RoutingModel`
- `NetworkModel`
- `LotSizingModel`
- `ScheduleModel`
- `AssignmentModel`
- `PackingModel`

Python bindings currently cover `RoutingModel`, `NetworkModel`, and `LotSizingModel`.

Example (`NetworkModel`):

```cpp
coso::NetworkModel m;
int s = m.add_node(5, "source");
int t = m.add_node(-5, "sink");
m.add_arc(s, t, /*cost=*/2, /*lower=*/0, /*upper=*/5);
auto r = m.solve(coso::TimeLimit(10));
```

## Canonical E2E Examples

One runnable C++ example per model family is available in
`examples/canonical/`:

- `examples/canonical/routing_example.cpp`
- `examples/canonical/network_example.cpp`
- `examples/canonical/lotsizing_example.cpp`
- `examples/canonical/schedule_example.cpp`
- `examples/canonical/assignment_example.cpp`
- `examples/canonical/packing_example.cpp`

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

## Roadmap

See [docs/roadmap.md](docs/roadmap.md) for the full design plan: modeling
interface, architecture, problem catalog (50+ problem types), implementation
steps, and design decisions.

## License

MIT — see [LICENSE](LICENSE).
