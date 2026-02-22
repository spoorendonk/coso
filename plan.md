# primal-rsp — Primal Heuristics for Routing, Scheduling & Production Planning

Declarative modeling + LP-free solving for structured combinatorial optimization.

Think MIP modeling (CPLEX/Gurobi) but for routing, scheduling, and planning:
the user declares **what** the problem is, the solver decides **how** to solve it.

Sibling to `mip-heuristics` (LP-free MIP solvers: FJ, Local-MIP). That repo
handles generic MIP (`Ax ≤ b`). This repo handles problems with **exploitable
structure** — routes, sequences, schedules — where problem-specific local search
dominates generic approaches by orders of magnitude.

---

## 1. User Experience

### 1.1 Standard CVRP — declare and solve

```cpp
#include <primal/routing.h>

primal::Model m;

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
auto result = m.solve(primal::TimeLimit(60));

// Use
std::cout << "Cost: " << result.cost() << "\n";
for (auto& route : result.routes()) {
    for (int c : route) std::cout << c << " ";
    std::cout << "\n";
}
```

Or from a CVRPLIB file:

```cpp
auto result = primal::solve("X-n101-k25.vrp", primal::TimeLimit(60));
```

**Zero implementation needed.** The solver recognizes: routes + capacity →
uses LoadSegment, standard operators, ILS/HGS.

### 1.2 VRPTW with heterogeneous fleet — just add parameters

```cpp
primal::Model m;
m.add_depot(0, 0, {.tw = {0, 1000}});

// Two vehicle types with different capacities and costs
m.add_vehicle_type(3, {.capacity = 100, .max_duration = 500});
m.add_vehicle_type(2, {.capacity = 200, .max_duration = 800, .fixed_cost = 50});

// Clients with time windows
m.add_client(10, 20, {.demand = 15, .tw = {100, 200}, .service = 10});
m.add_client(30, 40, {.demand = 25, .tw = {150, 300}, .service = 15});
m.add_client(50, 60, {.demand = 10, .pickup = 5});  // pickup-delivery

auto result = m.solve(primal::TimeLimit(60));
```

**Still zero implementation.** The model sees time windows → adds DurationSegment.
Sees heterogeneous fleet → adds vehicle-type-aware evaluation. The user just
declares attributes, never touches the engine.

### 1.3 Job shop scheduling — same pattern, different domain

```cpp
#include <primal/scheduling.h>

primal::ScheduleModel m;

// Jobs with ordered operations
auto j1 = m.add_job();
m.add_operation(j1, {.machine = 0, .duration = 3});
m.add_operation(j1, {.machine = 1, .duration = 2});

auto j2 = m.add_job();
m.add_operation(j2, {.machine = 1, .duration = 4});
m.add_operation(j2, {.machine = 0, .duration = 1});

m.minimize_makespan();
auto result = m.solve(primal::TimeLimit(30));
```

Or from a standard format:

```cpp
auto result = primal::solve_jsp("tai20x15.txt", primal::TimeLimit(30));
```

### 1.4 Lot sizing — MIP substructure, delegates to mip-heuristics

```cpp
#include <primal/lotsizing.h>

primal::LotSizingModel m;
auto p1 = m.add_product({.setup_cost = 100, .holding_cost = 2});
auto p2 = m.add_product({.setup_cost = 150, .holding_cost = 3});

m.add_demand(p1, {.period = 0, .quantity = 50});
m.add_demand(p1, {.period = 1, .quantity = 30});
m.add_demand(p2, {.period = 0, .quantity = 40});

m.set_capacity({200, 200, 200});  // per-period capacity

auto result = m.solve(primal::TimeLimit(60));
```

### 1.5 Custom constraints — extend when needed

For constraints the model doesn't know about, the user provides a penalty function:

```cpp
primal::Model m;
// ... standard setup ...

// Custom: each client requires a skill, vehicles have skill sets
m.add_client_attribute("required_skill", {1, 2, 1, 3, 2, ...});
m.add_vehicle_attribute("skills", {{1,2}, {2,3}, {1,3}, ...});

// Penalty function: called for each route to compute violation
m.add_route_penalty("skill_mismatch", [](const auto& route, const auto& data) {
    int violations = 0;
    for (int c : route.customers()) {
        int skill = data.client_attr<int>("required_skill", c);
        auto& vskills = data.vehicle_attr<std::vector<int>>("skills", route.vehicle());
        if (std::find(vskills.begin(), vskills.end(), skill) == vskills.end())
            violations++;
    }
    return violations;
});

auto result = m.solve(primal::TimeLimit(60));
```

This is **slower** than a native segment (callback overhead on every move
evaluation). For production use with custom constraints, the user can implement
a segment type in C++ for full performance. But the lambda approach lets them
prototype quickly.

---

## 2. What the Model Recognizes (Out of the Box)

The modeling layer maps declared attributes to internal engine components
automatically:

### Routing

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| `demand` + `capacity` | Capacitated VRP | LoadSegment |
| `tw` + `service` | Time windows | DurationSegment |
| `max_duration` on vehicle | Max route duration | DurationSegment |
| Multiple `add_vehicle_type` | Heterogeneous fleet | Vehicle-type-aware eval |
| Multiple `add_depot` | Multi-depot | Start/end depot per vehicle |
| `pickup` on client | Pickup-delivery | LoadSegment extension |
| `required = false` + `prize` | Optional clients | Prize collection |
| `open = true` on vehicle | Open routes | No return-to-depot |

### Scheduling

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| Operations with `machine` + `duration` | Job shop | Disjunctive graph |
| Same machine order for all jobs | Flow shop | Permutation schedule |
| `setup_time` between ops | Sequence-dependent setup | Extended evaluation |
| `due_date` on job | Tardiness | Penalty term |
| `release_date` on job | Release dates | Feasibility check |
| Multiple machines per op | Flexible job shop | Assignment + sequencing |

### Lot Sizing

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| Products + demands + capacity | CLSP | Fix-and-Optimize |
| `setup_cost` + `holding_cost` | Setup + inventory | MIP objective |
| Multi-level BOM | MLCLSP | Multi-level decomposition |

---

## 3. Internal Architecture

The user never sees this. The Model translates their declaration into engine
components.

### 3.1 Three-layer engine

```
┌─────────────────────────────────────────────────────────┐
│  Search Control (ILS, HGS, SA)                          │
│  → Each algorithm is its own class, no forced interface │
│  → Reusable across problem types                        │
├─────────────────────────────────────────────────────────┤
│  Move Evaluation (segments + CostEvaluator)             │
│  → Extensibility point for new constraints              │
│  → New constraint = new segment type with merge()       │
├─────────────────────────────────────────────────────────┤
│  Move Topology (operators)                              │
│  → How nodes/jobs get rearranged                        │
│  → Reusable across constraint variants                  │
│  → Clean function signatures for LLM operator discovery │
└─────────────────────────────────────────────────────────┘
```

- **Operators** describe topology changes (relocate, swap, 2-opt). They don't
  know about constraints. Reusable across all VRP variants.
- **Segments** propagate constraint state along routes via `merge()`. Adding a
  new constraint = adding a segment type. Operators unchanged.
- **CostEvaluator** sums penalty terms from all active segments. Operators call
  it to price out moves.
- **Algorithms** (ILS, HGS) compose operators + local search + penalty management.
  Each is its own class.

### 3.2 Segment concatenation (the extensibility mechanism)

Learned from PyVRP (Wouda et al., 2024) and UHGS (Vidal et al., 2014):

```cpp
struct LoadSegment {
    int delivery, pickup, load;
    static LoadSegment merge(LoadSegment a, LoadSegment b);
    int excess_load(int capacity) const;
};

struct DurationSegment {
    int duration, time_warp, tw_early, tw_late;
    static DurationSegment merge(DurationSegment a, DurationSegment b,
                                  int edge_duration);
    int total_time_warp() const;
};
```

When the Model sees `demand` + `capacity`, it activates `LoadSegment`.
When it sees `tw`, it activates `DurationSegment`. The user doesn't know
these exist.

### 3.3 LLM-friendly operator interfaces

For LLM-driven heuristic discovery (VRPAgent, EoH, FunSearch):

```cpp
// Clean function signatures LLMs can target
using DestroyFn = std::function<std::vector<int>(Solution&, int count, RNG&)>;
using RepairFn  = std::function<void(Solution&, const std::vector<int>&,
                                      const ProblemData&)>;
using PerturbFn = std::function<Solution(const Solution&, const ProblemData&, RNG&)>;
```

---

## 4. Source Layout

```
src/
  model/              ← User-facing modeling API
    model.h               Routing model (add_depot, add_client, add_vehicle_type)
    schedule_model.h      Scheduling model (add_job, add_operation)
    lotsizing_model.h     Lot sizing model
    instance_reader.h     CVRPLIB, Solomon, Taillard parsers

  routing/            ← CVRP/VRPTW engine (internal)
    problem_data.h        Compiled instance data
    solution.h            Route-based solution
    route.h               Route with segment-based evaluation
    segments/
      load_segment.h
      duration_segment.h
    cost_evaluator.h
    operators/            Relocate, Swap, SWAP*, 2-opt, Or-opt, Cross
    perturbation/         Destroy + Repair operators
    local_search.h
    penalty_manager.h
    neighbours.h

  search/             ← Metaheuristic shells
    iterated_local_search.h
    genetic_algorithm.h
    population.h
    crossover.h
    stop_criterion.h
    acceptance.h

  scheduling/         ← JSP/FSP engine (Phase 5)
  lotsizing/          ← CLSP engine (Phase 6)

  cli/
    main.cpp

tests/
  model/              ← User-facing tests (the API contract)
  routing/
  search/
  data/
```

---

## 5. Implementation Phases

### Phase 1 — CVRP end-to-end

User can: `m.add_depot(); m.add_client({.demand=...}); m.solve();`

Engine:
1. Repo skeleton (CMake, C++23, Catch2)
2. `Model` class with `add_depot`, `add_client`, `add_vehicle_type`, `solve`
3. CVRPLIB parser
4. `ProblemData` (compiled from Model)
5. `LoadSegment` with `merge()`
6. `Route` with prefix segment arrays
7. `Solution` + `CostEvaluator`
8. Operators: Relocate(1,0), Swap(1,1), 2-opt, Or-opt
9. `LocalSearch` engine (granular neighbourhood)
10. Construction heuristic (nearest-neighbour or savings)
11. ILS (ruin-and-recreate + late acceptance)
12. Tests: model API, operator correctness, small instance end-to-end

### Phase 2 — Benchmark quality

1. SWAP* operator
2. PenaltyManager (adaptive α)
3. Uchoa X-set benchmarks. Target: <2% gap, 60s.

### Phase 3 — HGS

1. Population (feasible + infeasible, diversity)
2. Crossover (SREX)
3. `GeneticAlgorithm`
4. Compare ILS vs HGS on X-set

### Phase 4 — VRPTW + rich VRP

User can: `m.add_client({.tw = {100,200}, .service = 10}); m.solve();`

Engine:
1. `DurationSegment` with `merge()`
2. Route carries duration segments
3. PenaltyManager adds β (time warp)
4. Heterogeneous fleet, max duration, multiple depots
5. Solomon + Gehring-Homberger benchmarks

### Phase 5 — Scheduling

User can: `m.add_job(); m.add_operation(j, {.machine=0, .duration=3});`

Engine: separate concrete types, search patterns carry over.

### Phase 6 — Lot sizing

User can: `m.add_product({.setup_cost=100}); m.add_demand(p, {.period=0, .qty=50});`

Engine: Fix-and-Optimize with mip-heuristics as inner solver.

### Phase 7 — Custom constraints

User can: `m.add_route_penalty("name", lambda)` for quick prototyping.
Power user can: implement a C++ segment type for full performance.

---

## 6. Relationship to mip-heuristics

```
mip-heuristics                        primal-rsp
┌──────────────────────────┐          ┌──────────────────────────┐
│                          │          │                          │
│ User provides:           │          │ User provides:           │
│   .mps file              │          │   Model declarations     │
│   (variables, Ax ≤ b)    │          │   (depots, clients,      │
│                          │          │    vehicles, jobs, ...)   │
│ Solver decides:          │          │                          │
│   FJ / Local-MIP         │          │ Solver decides:          │
│   move scoring           │          │   segments, operators,   │
│   weight updates         │          │   ILS / HGS, penalties   │
│                          │          │                          │
│ Generic: any MIP         │◄─────────│ Structured: RSP          │
│                          │ Phase 6  │                          │
└──────────────────────────┘          └──────────────────────────┘

Same pattern: user declares WHAT, solver decides HOW.
```

---

## 7. Key Design Decisions

1. **Model is the product.** The user interacts with `Model`, `ScheduleModel`,
   `LotSizingModel`. They never see segments, operators, or algorithms.

2. **Attribute-driven engine selection.** When the user sets `tw` on a client,
   the Model activates `DurationSegment` internally. No explicit configuration.

3. **Integer arithmetic in the hot path.** Scale and round distances. CVRPLIB
   uses integers. Avoids floating-point. Cache-friendly.

4. **Granular neighbourhood.** k-nearest (k=40). O(n·k) not O(n²).

5. **Penalized cost.** `distance + α·excessLoad + β·timeWarp + ...` Infeasible
   intermediate solutions. Adaptive penalty weights.

6. **Operator ordering.** Cheapest-first: Relocate, Swap, 2-opt, Or-opt, SWAP*.

7. **No ALNS.** ILS and HGS outperform it. ALNS adds complexity without quality.

8. **No abstract Algorithm base.** ILS and HGS are separate classes. Custom
   algorithms use components directly.

9. **Lambda penalties for quick prototyping.** Not as fast as native segments,
   but lets users add custom constraints without C++.

10. **Clean function signatures for LLM targeting.** `DestroyFn`, `RepairFn`
    are simple enough for LLM code generation.

---

## 8. References

Core methods:
- Vidal et al. (2014). *A unified solution framework for multi-attribute VRPs*. C&OR.
- Vidal (2022). *HGS for the CVRP: SWAP\**. C&OR.
- Wouda et al. (2024). *PyVRP: a high-performance VRP solver package*. IJOC.
- Máximo et al. (2024). *AILS-II: Adaptive ILS for large-scale CVRP*. IJOC.

Benchmarks:
- Uchoa et al. (2017). *New benchmark instances for the CVRP*. EJOR.
- CVRPLIB BKS Challenge (2026). https://vrp.galgos.inf.puc-rio.br/

Scheduling:
- Nowicki & Smutnicki (1996). *A fast taboo search for the job shop*. MS.

Lot sizing:
- Helber & Sahling (2010). *Fix-and-optimize for CLSP*. IJPE.
- Muller, Spoorendonk & Pisinger (2012). *Hybrid ALNS for lot-sizing*. EJOR.

LLM/Neural:
- Ye et al. (2025). *VRPAgent: LLM-driven operator discovery*. arXiv.
- Liu et al. (2024). *Evolution of Heuristics (EoH)*. ICML.
- Romera-Paredes et al. (2024). *FunSearch*. Nature.
