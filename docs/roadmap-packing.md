# Packing Roadmap

Content extracted from the [main roadmap](roadmap.md) for the bin packing engine.

---

## API Examples

### Bin packing — items into bins

```cpp
#include <coso/packing.h>

coso::PackingModel m;

// Bin type with capacity
m.add_bin_type(100, {.capacity = {150, 200}});  // 100 bins, weight + volume

// Items with sizes
m.add_item({.size = {30, 40}});
m.add_item({.size = {50, 60}});
m.add_item({.size = {20, 30}});
// ...

m.minimize_bins();
auto result = m.solve(coso::TimeLimit(30));

for (auto& bin : result.bins())
    for (int item : bin) std::cout << item << " ";
```

For vector bin packing (multiple dimensions) and bin packing with conflicts
(incompatible item pairs), just add attributes:

```cpp
m.add_conflict(item_a, item_b);  // cannot share a bin
```

**Same assignment engine underneath.** Items assigned to bins = entities assigned
to values. Partition constraint implicit (each item in exactly one bin). Operators:
MoveItem (reassign), SwapItems (exchange between bins). Capacity tracked per bin.

---

## Attribute Mapping

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| Items + bins + capacity | Bin packing | Assignment engine (move/swap items) |
| Multi-dim `size` + `capacity` | Vector bin packing | Multi-dim capacity per bin |
| `conflict(a, b)` | Bin packing with conflicts | Conflict constraint |
| `minimize_bins()` | Minimize bins used | Bin count objective |

---

## Problem Catalog

| # | Problem | Abbrev | Approach | Phase | Benchmarks | Instances | Source |
|---|---|---|---|---|---|---|---|
| K1 | Bin Packing | BPP | Assignment engine (move/swap) | 6+ | Scholl, Falkenauer | ~1,370 | BPPLIB |
| K2 | Vector Bin Packing | VBP | Multi-dim capacity per bin | 6+ | Caprara-Toth | varies | Papers |
| K3 | Bin Packing w/ Conflicts | BPPC | + conflict constraint | 8+ | Muritiba et al. | varies | Papers |

---

## Instance Formats

| Format | Covers | Parser needed |
|---|---|---|
| BPPLIB | BPP, VBP | Yes (Phase 6+) |

---

## Work Units

### Step 9 — Bin packing engine

```
Deliverable: solve bin packing from PackingModel API.
Touches only src/packing/ — parallel with step 7, 8.
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 9.1 | PackingModel implementation | Model → compiled instance for packing engine | src/model/packing_model.cpp | 1.3 |
| 9.2 | Packing solution representation | Bin assignments, load tracking, cost eval | src/packing/packing_solution.{h,cpp} | 9.1 |
| 9.3 | Packing operators | Move-item, swap-items, split-bin, merge-bin | src/packing/packing_operators.{h,cpp} | 9.2 |
| 9.4 | Bin capacity tracking | Multi-dimensional capacity with incremental updates | src/packing/bin_capacity.{h,cpp} | 9.2 |
| 9.5 | Packing benchmarks | Falkenauer, Scholl bin packing gap tests | tests/packing/ | 9.3 |
