# Lot Sizing Roadmap

## API Examples

### 1.10 Lot sizing — MIP substructure, delegates to mip-heuristics

```cpp
#include <coso/lotsizing.h>

coso::LotSizingModel m;
auto p1 = m.add_product({.setup_cost = 100, .holding_cost = 2});
auto p2 = m.add_product({.setup_cost = 150, .holding_cost = 3});

m.add_demand(p1, {.period = 0, .quantity = 50});
m.add_demand(p1, {.period = 1, .quantity = 30});
m.add_demand(p2, {.period = 0, .quantity = 40});

m.set_capacity({200, 200, 200});  // per-period capacity

auto result = m.solve(coso::TimeLimit(60));
```

---

## Attribute Mapping

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| Products + demands + capacity | CLSP | Fix-and-Optimize |
| `setup_cost` + `holding_cost` | Setup + inventory | MIP objective |
| Multi-level BOM | MLCLSP | Multi-level decomposition |

---

## Problem Catalog

| # | Problem | Abbrev | Approach | Phase | Benchmarks | Instances | Source |
|---|---|---|---|---|---|---|---|
| P1 | Capacitated Lot Sizing | CLSP | Fix-and-Optimize (MIP) | 6 | Trigeiro | 540 | Suerie |
| P2 | Multi-Level CLSP | MLCLSP | Fix-and-Optimize (MIP) | 6 | Tempelmeier-Derstroff | ~1920 | Uni Cologne† |

† Original URLs may be defunct. Instances typically regenerated from published parameters.

---

## Instance Formats

No specific instance formats listed for lot sizing problems.

---

## Work Units

### Step 10 — Advanced features (lot sizing)

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 10.9 | Lot sizing engine | CLSP/MLCLSP, delegates to mip-heuristics | src/lotsizing/ | 1.2 | **Done** |

### Step 12 — Public model API completion (lot sizing)

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 12.2 | LotSizingModel public API | `src/model/lotsizing_model.{h,cpp}` delegating to lotsizing engine / mip-heuristics bridge | src/model/, src/lotsizing/, tests/lotsizing/ | 10.9 |
