# COSO — Combinatorial Structure-aware Optimization

> **Disclaimer:** This project was developed entirely through [Claude Code](https://docs.anthropic.com/en/docs/claude-code). 😱

Declarative modeling + LP-free solving for combinatorial optimization.
C++ engine with Python bindings (nanobind).

The user declares **what** the problem is, the solver decides **how** to solve it.

Sibling to [mip-heuristics](https://github.com/spoorendonk/mip-heuristics) (LP-free
MIP solvers: FJ, Local-MIP). That repo handles generic MIP. This repo handles
problems with exploitable structure where problem-specific local search dominates
generic approaches by orders of magnitude.

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

| Engine | Problems | Approach |
|--------|----------|----------|
| **Routing** | CVRP, VRPTW, PDPTW, TRSP, fleet, multi-trip, ... | Resources + ILS/HGS |
| **Scheduling** | JSP, FJSP, RCPSP, flow shop, open shop, ... | Disjunctive graph + tabu |
| **Assignment** | Nurse rostering, timetabling, employee scheduling | Tabu + LA + VND + CP filter |
| **Packing** | Bin packing, vector bin packing | Assignment engine + FFD |
| **Network** | MCF, RCMCF, liner shipping | MCF + network local search |
| **Lot sizing** | CLSP, MLCLSP | Constructive + lot-sizing operators |

## Public Model APIs

All engines are available through typed model APIs in C++ and Python:

- `RoutingModel`
- `NetworkModel`
- `LotSizingModel`
- `ScheduleModel`
- `AssignmentModel`
- `PackingModel`

Example (`NetworkModel`):

```cpp
coso::NetworkModel m;
int s = m.add_node(5, "source");
int t = m.add_node(-5, "sink");
m.add_arc(s, t, /*cost=*/2, /*lower=*/0, /*upper=*/5);
auto r = m.solve(coso::TimeLimit(10));
```

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
