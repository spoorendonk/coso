# Routing Roadmap

## API Examples

### 1.1 Standard CVRP — declare and solve

```cpp
#include <coso/routing_model.h>

coso::RoutingModel m;

// Structure
auto depot = m.add_depot(456, 320);
auto vtype = m.add_vehicle_type(4, {.capacity = 15});

// Customers
m.add_client(228, 0, {.demand = 1});
m.add_client(912, 0, {.demand = 1});
m.add_client(0,   80, {.demand = 3});
// ...

// Distances auto-computed from coordinates (Euclidean, rounded)
// Or: m.set_distance(i, j, dist);

// Solve
auto result = m.solve(coso::TimeLimit(60));

// Use
std::cout << "Cost: " << result.cost() << "\n";
for (auto& route : result.routes()) {
    for (int c : route) std::cout << c << " ";
    std::cout << "\n";
}
```

Or from a CVRPLIB file:

```cpp
auto result = coso::solve("X-n101-k25.vrp", coso::TimeLimit(60));
```

**Zero implementation needed.** The solver recognizes: routes + capacity →
uses LoadResource, standard operators, ILS/HGS.

**Python equivalent:**

```python
import coso

m = coso.RoutingModel()
depot = m.add_depot(456, 320)
vtype = m.add_vehicle_type(4, capacity=15)
m.add_client(228, 0, demand=1)
# ...
result = m.solve(coso.TimeLimit(60))
```

### 1.2 VRPTW with heterogeneous fleet — just add parameters

```cpp
coso::RoutingModel m;
m.add_depot(0, 0, {.tw = {0, 1000}});

// Two vehicle types with different capacities and costs
m.add_vehicle_type(3, {
    .capacity = {100, 50},       // weight, volume (multiple dimensions)
    .max_duration = 500,
    .fixed_cost = 20,
    .unit_distance_cost = 1,
    .unit_duration_cost = 2,
});
m.add_vehicle_type(2, {
    .capacity = {200, 80},
    .max_duration = 800,
    .fixed_cost = 50,
    .unit_distance_cost = 1,
    .profile = 1,                // different distance/duration matrix
});

// Clients with time windows
m.add_client(10, 20, {.demand = {15, 5}, .tw = {100, 200}, .service = 10});
m.add_client(30, 40, {.demand = {25, 8}, .tw = {150, 300}, .service = 15});
m.add_client(50, 60, {.demand = {10, 3}, .pickup = {0, 2}});

// Routing profiles: different distance/duration matrices per vehicle type
m.set_profile(0);  // default profile (already set by set_distance)
m.set_profile_distance(1, i, j, dist);  // profile for heavy vehicles
m.set_profile_duration(1, i, j, dur);

auto result = m.solve(coso::TimeLimit(60));
```

**Still zero implementation.** The RoutingModel sees time windows → adds DurationResource.
Sees heterogeneous fleet → adds vehicle-type-aware evaluation. Multiple load
dimensions → LoadResource tracks all dimensions. The user just declares
attributes, never touches the engine.

### 1.3 Multi-trip with overtime

```cpp
coso::RoutingModel m;
auto depot = m.add_depot(0, 0, {.tw = {0, 480}});

m.add_vehicle_type(3, {
    .capacity = 100,
    .max_duration = 480,
    .max_overtime = 60,          // can exceed shift by up to 60
    .unit_overtime_cost = 5,     // at 5x cost per unit
    .reload_depot = depot,       // return to depot to reload
    .max_reloads = 2,            // up to 2 reloads per shift
});

m.add_client(10, 20, {.demand = 40, .tw = {100, 200}, .service = 10});
// ...

auto result = m.solve(coso::TimeLimit(60));
```

### 1.4 Paired pickup-delivery with time windows

```cpp
coso::RoutingModel m;
auto depot = m.add_depot(0, 0, {.tw = {0, 1000}});
m.add_vehicle_type(5, {.capacity = 20});

// Request: pick up at location A, deliver to location B (same route, in order)
auto p1 = m.add_pickup(10, 20, {.quantity = 5, .tw = {100, 200}, .service = 10});
auto d1 = m.add_delivery(30, 40, {.tw = {150, 300}, .service = 10});
m.add_request(p1, d1);  // p1 and d1 on same route, p1 before d1

auto p2 = m.add_pickup(50, 60, {.quantity = 8, .tw = {200, 400}});
auto d2 = m.add_delivery(70, 80, {.tw = {250, 500}});
m.add_request(p2, d2);

auto result = m.solve(coso::TimeLimit(60));
```

### 1.5 Optional clients and client groups

```cpp
coso::RoutingModel m;
// ...

// Optional clients with prizes (Team Orienteering)
m.add_client(10, 20, {.demand = 5, .required = false, .prize = 100});

// Client groups: exactly one from each group must be served
auto g1 = m.add_client_group();
m.add_client(10, 20, {.demand = 5, .group = g1});  // alternative locations
m.add_client(12, 22, {.demand = 5, .group = g1});  // for same customer

// Release times: client not available until a given time
m.add_client(30, 40, {.demand = 10, .release_time = 120});

auto result = m.solve(coso::TimeLimit(60));
```

---

## Attribute Mapping

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| `demand` + `capacity` | Capacitated VRP | LoadResource |
| Multi-dim `demand` + `capacity` | Multi-dimensional capacity | LoadResource (N dims) |
| `tw` + `service` | Time windows | DurationResource |
| `release_time` on client | Release times | DurationResource |
| `max_duration` on vehicle | Max route duration | DurationResource |
| `max_distance` on vehicle | Max route distance | DistanceResource |
| `max_overtime` + `unit_overtime_cost` | Overtime | DurationResource (soft bound) |
| `fixed_cost` on vehicle | Fixed vehicle cost | CostEvaluator |
| `unit_distance_cost` on vehicle | Variable distance cost | CostEvaluator |
| `unit_duration_cost` on vehicle | Variable duration cost | CostEvaluator |
| Multiple `add_vehicle_type` | Heterogeneous fleet | Vehicle-type-aware eval |
| `profile` on vehicle type | Routing profiles | Per-profile distance/duration |
| Multiple `add_depot` | Multi-depot | Start/end depot per vehicle |
| `start_depot` ≠ `end_depot` | Asymmetric depot | Depot per route endpoint |
| `pickup` on client | Simultaneous pickup-delivery | PickupDeliveryResource |
| `add_request(pickup, delivery)` | Paired pickup-delivery | PrecedenceResource |
| `required = false` + `prize` | Optional clients | Prize collection |
| `group` on client | Mutually exclusive groups | Group operators |
| `open = true` on vehicle | Open routes | No return-to-depot |
| `reload_depot` + `max_reloads` | Multi-trip | Reload-aware route structure |
| `initial_load` on vehicle | Pre-loaded cargo | LoadResource init |
| `cluster` on client | Clustered VRP | ClusterResource |
| `transshipment` on depot | Transshipment facility | Multi-echelon routing |
| `break_duration` + `max_interbreak` | Driver breaks | BreakResource |
| `pickup_policy = LIFO\|FIFO` | Loading order | PrecedenceResource (extended) |
| `type_incompatibility(A, B)` | Hazmat / type conflicts | TypeIncompatibilityResource |
| `type_requirement(A, B)` | Co-presence requirements | TypeRequirementResource |
| `depot_capacity` on depot | Loading dock limits | DepotResourceConstraint |
| `span_cost` on dimension | Minimize max route span | Global span objective |
| `skills` on client + vehicle | Skills matching | SkillFilter (precomputed) |
| `setup` on client | Setup time (location-aware) | DurationResource (skip same loc) |
| Multiple `tw` per client | Multiple time windows | DurationResource (TW array) |
| `max_tasks` on vehicle | Max stops per vehicle | TaskCountResource |
| `speed_factor` on vehicle | Vehicle speed multiplier | Duration scaling |
| `service_per_type` on client | Type-specific service time | Per-vehicle-type lookup |
| `cost_matrix` per profile | Separate cost matrix | CostEvaluator (3 matrices) |
| `unit_task_duration_cost` | Per-task-hour cost | CostEvaluator |
| `skills` + `tw` + `teams` | Technician routing & scheduling | SkillFilter + DurationResource + team formation |
| `synchronization(task, n)` | Synchronized visits | SyncResource (n techs at same time) |

---

## Problem Catalog

| # | Problem | Abbrev | Approach | Phase | Benchmarks | Instances | Source |
|---|---|---|---|---|---|---|---|
| R1 | Capacitated VRP | CVRP | LoadResource | 1 | Uchoa X-set, A/B/E/Golden, XL | ~379 | CVRPLIB |
| R2 | VRP with Time Windows | VRPTW | + DurationResource | 4 | Solomon, Gehring-Homberger | ~356 | SINTEF |
| R3 | Heterogeneous Fleet VRP | HFVRP | Vehicle-type-aware eval | 4 | Golden, Taillard, PyVRP | ~128 | PyVRP/Instances |
| R4 | Multi-Depot VRP | MDVRP | Multi-depot routing | 4 | Cordeau-Gendreau-Laporte | 33 | NEO/UMA |
| R5 | Open VRP | OVRP | No return-to-depot | 4 | Li-Golden-Wasil, adapted CVRP | ~8+ | VRP-REP |
| R6 | VRP Simultaneous PD | VRPSPD | PickupDeliveryResource | 4 | Dethloff, Salhi-Nagy | ~133 | Papers |
| R7 | VRP with Backhauls | VRPB | LoadResource + precedence | 4 | Goetschalckx-Jacobs-Blecha | 62+90 | PyVRP/Instances |
| R8 | Team Orienteering | TOP | Optional + prize + duration | 4 | Chao et al. | ~387 | KU Leuven |
| R9 | Multi-dim Capacity | MDCVRP | LoadResource (N dims) | 4 | Adapted CVRP | varies | — |
| R10 | Routing Profiles | — | Per-profile matrices | 4 | — | — | — |
| R11 | Client Groups | GVRP | Group operators | 4 | — | varies | PyVRP |
| R12 | Multi-trip VRP | MTVRP | Reload-aware routes | 4b | Cattaruzza et al. | ~90 | PyVRP/Instances |
| R13 | Release Times | — | DurationResource ext | 4 | Solomon + release | varies | — |
| R14 | Overtime | — | Soft duration bound | 4 | — | — | — |
| R15 | Paired Pickup-Delivery TW | PDPTW | PrecedenceResource | 4c | Li-Lim, Sartori | ~900 | SINTEF |
| R16 | Capacitated Arc Routing | CARP | Transform to node routing | 5+ | gdb, egl, bccm | ~87 | DIMACS |
| R17 | Cumulative CVRP | CCVRP | CumulativeCostResource | 5+ | Adapted CVRP | ~100 | Mendeley |
| R18 | Split Delivery VRP | SDVRP | Split-aware repr ⚠️ | 5+ | DIMACS SDVRP | varies | DIMACS |
| R19 | Time-Dependent VRP | TDVRP | Time-dep distances | 5+ | DIMACS TDCARP | varies | DIMACS |
| R20 | Electric VRP | EVRP | BatteryResource + recharge | 5+ | Schneider et al., DIMACS | ~92+ | DIMACS |
| R21 | Period VRP | PVRP | Multi-period visits | 5+ | Cordeau et al. | ~30 | NEO/UMA |
| R22 | Inventory Routing | IRP | Routing + inventory | 5+ | DIMACS IRP | varies | DIMACS |
| R23 | Location-Routing | LRP | Depot selection + routing | 5+ | Prodhon, Prins | varies | Papers |
| R24 | Site-Dependent VRP | SDVRPTW | Vehicle access restrictions | 4 | Vidal et al. | varies | PyVRP/Instances |
| R25 | Clustered VRP | cluVRP | ClusterResource | 8+ | Battarra et al., Expósito | ~300+ | Papers |
| R26 | VRP with Transshipment | VRPTF | Multi-echelon routing | 8+ | Baldacci et al. | varies | Papers |
| R27 | Technician Routing & Scheduling | TRSP | Skills + TW + teams | 5 | ROADEF 2007, Solomon-TRSP | 36+ | HAL/ROADEF |
| R28 | Home Healthcare Routing | HHCRP | Skills + TW + sync | 5+ | Mankowska et al. | varies | Papers |

---

## Instance Formats

| Format | Covers | Parser needed |
|---|---|---|
| VRPLIB (TSPLIB extension) | CVRP, VRPTW, HFVRP, VRPB, MDVRP, MTVRP, TOP | Yes (Phase 1) |
| Solomon | VRPTW, PDPTW (Li-Lim) | Yes (Phase 4) |
| Cordeau | MDVRP, PVRP | Yes (Phase 4) |
| ROADEF 2007 | TRSP | Yes (Phase 5) |
| CARP format | CARP | Later |
| DIMACS VRP | SDVRP, EVRP, IRP, CARP | Later |

---

## Work Units

### Step 2 — CVRP end-to-end

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 2.1 | ~~CVRPLIB instance reader~~ | ~~Parse .vrp files into structured data~~ | ~~src/model/instance_reader.{h,cpp}, tests~~ | ~~1.2~~ |
| 2.2 | ProblemData (compiled instance) | Distance matrix, client/depot/vehicle attributes from model | src/routing/problem_data.{h,cpp}, tests | Done |
| 2.3 | ~~Route + LoadResource~~ | ~~Route with prefix/suffix resource arrays, LoadResource with init/merge/excess~~ | ~~src/routing/route.{h,cpp}, src/routing/resources/load_resource.h, tests~~ | ~~Done~~ |
| 2.4 | ~~Solution + CostEvaluator~~ | ~~Multi-route solution, objective + penalty evaluation~~ | ~~src/routing/solution.{h,cpp}, src/routing/cost_evaluator.{h,cpp}, tests~~ | ~~Done~~ |
| 2.5 | ~~Exchange operators~~ | ~~Exchange(1,0), (1,1), (2,0), SwapTails with O(1) move eval~~ | ~~src/routing/operators/exchange.{h,cpp}, tests~~ | ~~Done~~ |
| 2.6 | ~~Local search engine~~ | ~~Granular neighbourhood (k=40), first-improvement descent over operators~~ | ~~src/routing/local_search.{h,cpp}, tests~~ | ~~Done~~ |
| 2.7 | ~~Construction heuristic~~ | ~~Nearest-neighbour + Clarke-Wright savings~~ | ~~src/routing/construction.{h,cpp}, tests~~ | ~~Done~~ |
| 2.8 | ~~ILS + stop criterion~~ | ~~Ruin-and-recreate + late acceptance, time/iter/no-improve limits~~ | ~~src/search/iterated_local_search.{h,cpp}, src/search/stop_criterion.{h,cpp}, tests~~ | ~~Done~~ |
| 2.9 | ~~RoutingModel implementation~~ | ~~Model → ProblemData → construct → ILS → Result~~ | ~~src/model/routing_model.cpp, tests~~ | ~~Done~~ |
| 2.10 | ~~CLI~~ | ~~`coso-solve instance.vrp --time-limit 60`~~ | ~~src/cli/main.cpp, tests~~ | ~~Done~~ |
| 2.11 | ~~Benchmark setup + first benchmarks~~ | ~~Download script, X-n101-k25 end-to-end test~~ | ~~tests/data/download_benchmarks.sh, tests/routing/benchmark_test.cpp~~ | ~~Done~~ |

**Parallel lanes:**
- 2.1 and 2.2 are parallel (both only need 1.2)
- 2.7 is parallel with 2.5/2.6 (both need 2.4, merge at 2.8)

### Step 3 — Routing benchmark quality

```
Deliverable: <2% gap on Uchoa X-set in 60s. Competitive with PyVRP.
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 3.1 | ~~SWAP* operator~~ | ~~SWAP* move with O(1) eval via best-insert cache~~ | ~~src/routing/operators/swap_star.{h,cpp}~~ | ~~Done~~ |
| 3.2 | ~~Extended exchange family~~ | ~~Exchange(N,M) for N,M up to 3~~ | ~~src/routing/operators/exchange.{h,cpp} (extend)~~ | ~~Done~~ |
| 3.3 | ~~Adaptive penalty manager~~ | ~~Auto-tune capacity/TW penalty weights per iteration~~ | ~~src/search/penalty_manager.{h,cpp}~~ | ~~Done~~ |
| 3.4 | ~~Composable acceptance criteria~~ | ~~Late acceptance, simulated annealing, record-to-record~~ | ~~src/search/acceptance.{h,cpp}~~ | ~~Done~~ |
| 3.5 | ~~Guided local search~~ | ~~GLS with edge penalties for diversification~~ | ~~src/search/guided_local_search.{h,cpp}~~ | ~~Done~~ |
| 3.6 | ~~Operator selector (MAB)~~ | ~~Multi-armed bandit for operator selection~~ | ~~src/search/operator_selector.{h,cpp}~~ | ~~Done~~ |
| 3.7 | ~~Score corruption detection~~ | ~~Debug assertions verifying incremental vs full recompute~~ | ~~src/search/score_assert.{h,cpp}~~ | ~~Done~~ |
| 3.8 | ~~Score explanation~~ | ~~Human-readable cost breakdown for debugging~~ | ~~src/search/score_analysis.{h,cpp}~~ | ~~Done~~ |
| 3.9 | ~~Benchmark harness~~ | ~~Automated benchmark runner with CSV output + gap reporting~~ | ~~src/search/benchmarker.{h,cpp}~~ | ~~Done~~ |

**All of 3.1–3.9 are parallel** — they touch different files. Integrate
together at the end for benchmark runs.

### Step 5 — VRPTW + rich VRP

```
Deliverable: full-featured routing with time windows, fleet, PD, etc.
```

**Resources (all parallel — each is a separate file):**

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 5.1 | Duration resource | Time window feasibility with wait/late tracking | src/routing/resources/duration_resource.h | 2.3 |
| 5.2 | Distance resource | Max distance / max duration per route | src/routing/resources/distance_resource.h | 2.3 |
| 5.3 | Precedence resource | Pickup-before-delivery within same route | src/routing/resources/precedence_resource.h | 2.3 |
| 5.4 | Break resource | Scheduled driver breaks within time windows | src/routing/resources/break_resource.h | 2.3 |
| 5.5 | Skill filter | Vehicle-client compatibility (skills, zones) | src/routing/resources/skill_filter.h | 2.3 |
| 5.6 | Type incompatibility | Clients that cannot share a route | src/routing/resources/type_incompatibility.h | 2.3 |
| 5.7 | Depot resource | Multi-depot assignment with open/close times | src/routing/resources/depot_resource.h | 2.3 | **Done** |
| 5.8 | Task count resource | Min/max clients per route | src/routing/resources/task_count_resource.h | 2.3 | **Done** |

**Operators + features (parallel where noted):**

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 5.9 | Pair operators | Relocate-pair, swap-pair for pickup-delivery | src/routing/operators/pair_operators.{h,cpp} | 5.3 |
| 5.10 | Multi-trip support | Relocate-with-depot-insert for multi-trip VRP | src/routing/operators/relocate_with_depot.{h,cpp} | 2.5 |
| 5.11 | Route split operator | Split long routes at optimal point | src/routing/operators/route_split.{h,cpp} | 2.5 | **Done** |
| 5.12 | Optional client handling | Insert/remove operators for optional visits | src/routing/operators/insert_optional.{h,cpp} | 2.5 |
| 5.13 | **Warm start + pinning** | **Initialize from existing solution, pin fixed clients** | **src/search/warm_start.{h,cpp}** | **2.8** |
| 5.14 | Rich VRP instance parsers | Solomon, Li-Lim, Gehring-Homberger parsers | src/model/instance_reader.cpp (extend) | 2.1 |
| 5.15 | VRPTW benchmarks | Solomon C1/R1/RC1 end-to-end gap tests | tests/routing/ (extend) | 5.1, 5.14 |

**5.1–5.8 are all parallel.** 5.9–5.13 are parallel. 5.14 is independent.

### Step 4 — HGS + portfolio

```
Deliverable: portfolio solver (ILS + HGS) with shared solution pool.
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 4.1 | Population with diversity management | Biased fitness with broken-pairs diversity | src/search/population.{h,cpp} | 2.4 | **Done** |
| 4.2 | SREX crossover | Selective route exchange crossover operator | src/search/crossover.{h,cpp} | 2.4 | **Done** |
| 4.3 | Genetic algorithm | HGS-style GA: select parents → crossover → educate → insert | src/search/genetic_algorithm.{h,cpp} | 4.1, 4.2, 2.6 | **Done** |
| 4.4 | Portfolio solver | ILS + HGS with shared solution pool, TBB parallel | src/search/portfolio.{h,cpp} | 4.3, 2.8 | **Done** |
| 4.5 | Solution finalizer | Post-optimization: inter-route moves at zero penalty | src/search/solution_finalizer.{h,cpp} | 2.4 | **done** |

4.1, 4.2, 4.5 are parallel. 4.3 merges 4.1+4.2. 4.4 merges everything.

### Step 10 — Advanced features (routing)

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 10.1 | Partitioned search | Decompose large instances into sub-problems | src/search/partitioned_search.{h,cpp} | 2.6 | **Done** |
| 10.2 | Daemon mode | Continuous solving with dynamic updates | src/search/daemon.{h,cpp} | 2.8 | **Done** |
| 10.3 | Routing replanning | Re-optimize with fixed/pinned clients, warm start | src/search/warm_start.cpp (extend) | 5.13 | **Done** |
| 10.5 | Overconstrained handling | Soft violations with cost penalties for infeasible instances | src/routing/, src/assignment/ (extend) | 2.4, 8.2 | **Done** |
| 10.6 | Piecewise linear costs | Non-linear distance/duration cost functions | src/routing/cost_evaluator.cpp (extend) | 2.4 | **Done** |
| 10.7 | Extended routing resources | Compartments, loading constraints, sync visits | src/routing/resources/ (new resources) | 2.3 | **Done** |
