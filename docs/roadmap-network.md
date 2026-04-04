# Network Roadmap

## API Examples

### 1.6 Multi-commodity flow — fractional paths

For problems where flow can be split across paths (multi-commodity flow, network
design), the model supports fractional path variables:

```cpp
#include <coso/network.h>

coso::NetworkModel m;

// Nodes and arcs with capacities
auto n0 = m.add_node();
auto n1 = m.add_node();
auto n2 = m.add_node();
m.add_arc(n0, n1, {.capacity = 10, .cost = 3});
m.add_arc(n0, n2, {.capacity = 8,  .cost = 5});
m.add_arc(n1, n2, {.capacity = 6,  .cost = 2});

// Commodities with source, sink, demand
m.add_commodity(n0, n2, {.demand = 7});
m.add_commodity(n1, n2, {.demand = 4});

// Resources on arcs (optional — for RCMCF)
m.add_arc_resource("bandwidth", {.arc_data = bw, .global_bound = max_bw});

auto result = m.solve(coso::TimeLimit(60));

// Fractional solution: flow per arc per commodity
for (auto& [commodity, paths] : result.flows()) {
    for (auto& [path, flow] : paths)
        std::cout << "Commodity " << commodity << ": flow " << flow << "\n";
}
```

This connects to flowty-core's column generation approach. For problems with
resource constraints on paths (RCMCF), the solver uses pricing with resource-
constrained shortest paths. For simpler MCF, direct LP or network simplex.

---

## Attribute Mapping

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| Arcs + commodities + demands | Multi-commodity flow | Network simplex / CG |
| Arc resources + bounds | Resource-constrained MCF | Column generation + RCSPP |
| Ports + vessels + demands + schedules | Liner shipping network design | Routing + scheduling + fleet assignment |

---

## Problem Catalog

| # | Problem | Abbrev | Approach | Phase | Benchmarks | Instances | Source |
|---|---|---|---|---|---|---|---|
| N1 | Multi-Commodity Flow | MCF | Network simplex / LP | 10+ | SNDlib, Canad | varies | SNDlib |
| N2 | Resource-Constrained MCF | RCMCF | Column generation + RCSPP | 10+ | — | varies | — |
| N3 | Liner Shipping Network Design | LSNDP | Routing + scheduling + fleet | 10+ | LINERLIB (7 base, 21 variants) | 21 | GitHub/LINERLIB |

These support fractional paths (flow split across routes). Unlike pure VRP where
each route is an integer path, MCF allows fractional flow on arcs. This is
meaningful for network design, telecom routing, and logistics network planning
where demand can be split. Column generation with pricing (as in flowty-core)
is the natural approach.

---

## Instance Formats

| Format | Covers | Parser needed |
|---|---|---|
| SNDlib | MCF | Later |
| LINERLIB | LSNDP | Later |

---

## Work Units

### Step 10 — Advanced features (network)

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 10.10 | Network flow engine | MCF/RCMCF, column generation for liner shipping | src/network/ | 1.2 | **Done** |

### Step 12 — Public model API completion (network)

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 12.1 | NetworkModel public API | `src/model/network_model.{h,cpp}` + `Result::flows()` integration + C++ tests | src/model/, tests/model/, tests/network/ | 10.10 |
