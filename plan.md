# primal-rsp — Primal Heuristics for Routing, Scheduling & Production Planning

Standalone C++ library for LP-free primal heuristics on structured combinatorial
optimization problems: vehicle routing, job shop scheduling, and lot sizing.

Sibling to `mip-heuristics` (LP-free MIP solvers: FJ, Local-MIP). That repo
handles generic MIP. This repo handles problems with **exploitable structure** —
routes, sequences, assignments — where problem-specific local search dominates.

---

## 1. Design Principles

### 1.1 Three clean separation layers

The architecture separates three concerns that change for different reasons:

```
┌─────────────────────────────────────────────────────────┐
│  Search Control (ILS, HGS, SA, custom)                  │
│  → How to explore the solution space                    │
│  → Reusable across problem types                        │
│  → Each algorithm is its own class, no forced interface │
├─────────────────────────────────────────────────────────┤
│  Move Evaluation (segments + CostEvaluator)             │
│  → What's the cost impact of a rearrangement            │
│  → EXTENSIBILITY POINT for new constraints              │
│  → New constraint = new segment type + penalty term     │
├─────────────────────────────────────────────────────────┤
│  Move Topology (operators)                              │
│  → How nodes/jobs get rearranged                        │
│  → Reusable across constraint variants                  │
│  → LLM-TARGETABLE for operator discovery                │
└─────────────────────────────────────────────────────────┘
```

This separation means:
- **New constraint** (max duration, pickup-delivery, heterogeneous fleet) →
  add a segment type + penalty term. Operators unchanged.
- **New operator** (LLM-discovered destroy/repair, novel move) →
  add an operator. Segments unchanged.
- **New algorithm** (custom hybrid, neural-guided search) →
  add a search controller. Everything else unchanged.
- **No heuristic is limited by generality** — algorithms use components directly,
  no virtual dispatch on the hot path.

### 1.2 Segment concatenation for extensible evaluation

Learned from PyVRP (Wouda et al., IJOC 2024) and UHGS (Vidal et al., 2014):

An operator describes a **topology change** (rearrange nodes). It doesn't know
about constraints. To evaluate the cost impact, it concatenates **segments**:

```cpp
// Each constraint defines how it propagates along a route segment
struct LoadSegment {
    int delivery, pickup, load;
    static LoadSegment merge(LoadSegment a, LoadSegment b);
    int excess_load(int capacity) const;
};

struct DurationSegment {
    int duration, time_warp, tw_early, tw_late;
    static DurationSegment merge(DurationSegment a, DurationSegment b, int edge_duration);
    int total_time_warp() const;
};

// CostEvaluator sums penalties from all active segment types
struct CostEvaluator {
    int load_penalty;    // per unit excess load
    int tw_penalty;      // per unit time warp
    int dist_penalty;    // per unit excess distance

    int delta_cost(RouteProposal& proposal, Route& old_route) const;
};
```

To add a new constraint (e.g., maximum stops per route, skill-based assignment):
1. Define a new segment type with a `merge()` operation
2. Add a penalty weight to `CostEvaluator`
3. Operators and search algorithms are untouched

This is how UHGS handles 29 VRP variants with one codebase (Vidal et al., 2014)
and how PyVRP won DIMACS + EURO-NeurIPS competitions.

### 1.3 No abstract Problem/Algorithm layer

The top solvers (HGS-CVRP, AILS-II, FILO, PyVRP) don't use abstract interfaces.
They use **component composition**: concrete data structures, pluggable operators,
shared local search engine.

Why:
- Virtual dispatch on evaluate/move-scoring kills performance
- ILS and HGS have fundamentally different structures — a common
  `Algorithm::run()` is either useless or leaky
- Each algorithm uses components directly, unconstrained

### 1.4 LLM-friendly operator interfaces

For LLM-driven heuristic discovery (VRPAgent, EoH, FunSearch), operators need
clean function signatures that an LLM can target:

```cpp
// Perturbation: Solution → Solution (simplest interface for LLM generation)
using PerturbFn = std::function<Solution(const Solution&, const ProblemData&, RNG&)>;

// Destroy: Solution → partial solution (remove customers)
using DestroyFn = std::function<std::vector<int>(Solution&, int count, RNG&)>;

// Repair: partial solution → Solution (reinsert removed customers)
using RepairFn = std::function<void(Solution&, const std::vector<int>& removed,
                                     const ProblemData&)>;
```

LLMs generate these functions. The framework evaluates them, scores them, and
can do evolutionary selection over a population of operator implementations
(the EoH/VRPAgent pattern).

---

## 2. Architecture

```
src/
  routing/                ← CVRP, VRPTW, and variants
    problem_data.h            Instance data (distance matrix, demands, TWs)
    solution.h                Route-based solution representation
    route.h                   Single route with segment-based evaluation
    segments/
      load_segment.h          Capacity tracking + merge
      duration_segment.h      Time window + duration tracking + merge
      // future: skill_segment.h, compartment_segment.h, ...
    cost_evaluator.h          Combines penalties from all segment types
    operators/
      node_operator.h         Base for node-level moves (topology only)
      route_operator.h        Base for route-level moves (topology only)
      relocate.h              Exchange(N,0)
      swap.h                  Exchange(N,M)
      swap_star.h             SWAP* (Vidal)
      two_opt.h               Intra-route 2-opt
      or_opt.h                Intra-route Or-opt(1,2,3)
      cross.h                 2-opt* / tail swap
    perturbation/
      destroy.h               Remove operators (random, worst, related, string)
      repair.h                Insert operators (greedy, regret-k)
      ruin_recreate.h         Compose destroy + repair
    local_search.h            Compose operators, run to local optimum
    penalty_manager.h         Adaptive penalty weights
    neighbours.h              Precomputed k-nearest neighbor lists
    cvrplib_reader.h          Parse CVRPLIB instance format

  search/                 ← Metaheuristic shells (shared across problem types)
    iterated_local_search.h
    genetic_algorithm.h
    population.h              Feasible + infeasible subpops, diversity
    crossover.h               Base + OX, SREX implementations
    stop_criterion.h          Time, iterations, no-improvement
    acceptance.h              Late acceptance, SA, convergent

  scheduling/             ← Job shop, flow shop (Phase 2)
    schedule_data.h
    schedule.h                Operation sequences per machine
    disjunctive_graph.h       Critical path computation
    operators/
      block_swap.h            N5 critical-path moves
      insertion.h             Operation reinsertion
    local_search.h            Scheduling-specific LS engine

  lotsizing/              ← CLSP with Fix-and-Optimize (Phase 3)
    ...

  cli/
    main.cpp

tests/
  routing/
  search/
  data/                       Benchmark instances
```

---

## 3. Core Types (Routing)

### 3.1 ProblemData

```cpp
struct ProblemData {
    int num_locations;         // 0 = depot, 1..n = customers
    int num_vehicles;
    int vehicle_capacity;

    // Flat distance matrix (integer, scaled)
    std::vector<int> dist;
    int distance(int i, int j) const;

    std::vector<int> demand;

    // VRPTW (optional, zeroed if pure CVRP)
    std::vector<int> tw_early, tw_late, service_time;

    // Precomputed granular neighbourhood
    std::vector<std::vector<int>> neighbours;
    void compute_neighbours(int k = 40);

    // Extension point: additional per-customer or per-vehicle attributes
    // added as new vectors when new constraints are introduced
};
```

### 3.2 Route with segment-based evaluation

```cpp
class Route {
public:
    const std::vector<int>& customers() const;  // ordered visit sequence

    // Segment data — one per customer position, enabling O(1) move evaluation
    // via prefix/suffix concatenation
    LoadSegment load_between(int start, int end) const;
    DurationSegment duration_between(int start, int end) const;

    // Aggregate route metrics (from segments)
    int total_distance() const;
    int excess_load(int capacity) const;
    int time_warp() const;

    // Modify
    void insert(int pos, int customer);
    void remove(int pos);
    void recompute_segments(const ProblemData& data);

private:
    std::vector<int> customers_;
    std::vector<LoadSegment> load_segments_;       // prefix segments
    std::vector<DurationSegment> dur_segments_;    // prefix segments
    int distance_ = 0;
};
```

### 3.3 Solution

```cpp
class Solution {
public:
    std::vector<Route> routes;

    int total_distance() const;
    int total_excess_load() const;
    int total_time_warp() const;
    bool is_feasible() const;

    int penalized_cost(const CostEvaluator& eval) const;
};
```

### 3.4 Operators (topology only)

Operators describe rearrangements. They use CostEvaluator + segments to
compute deltas, but the topology logic is constraint-agnostic:

```cpp
class NodeOperator {
public:
    virtual ~NodeOperator() = default;

    // Evaluate best move involving node at position u_pos in route r1
    // and position v_pos in route r2. Returns cost delta via CostEvaluator.
    virtual int evaluate(int u_pos, int v_pos,
                         const Route& r1, const Route& r2,
                         const CostEvaluator& eval,
                         const ProblemData& data) = 0;

    virtual void apply(Solution& sol) = 0;
};
```

The operator's `evaluate` constructs route proposals (segment concatenations)
and calls `CostEvaluator::delta_cost()`. The CostEvaluator doesn't know what
operator called it — it just prices out the proposal.

### 3.5 CostEvaluator

```cpp
class CostEvaluator {
public:
    int load_penalty;       // α — per unit excess load
    int tw_penalty;         // β — per unit time warp
    // Future: distance_penalty, stops_penalty, skill_penalty, ...

    // Price out a route proposal vs current route
    template<bool exact = true>
    int delta_cost(const RouteProposal& proposal,
                   const Route& current) const;

    // Price out a two-route proposal (inter-route moves)
    template<bool exact = true>
    int delta_cost(const RouteProposal& p1, const Route& r1,
                   const RouteProposal& p2, const Route& r2) const;

    int penalized_cost(const Route& r) const;
};
```

When `exact=false`, delta_cost can exit early if the partial sum is already
non-improving (≥0). This is a critical optimization — most moves are rejected.

### 3.6 LocalSearch

```cpp
class LocalSearch {
public:
    LocalSearch(const ProblemData& data, const CostEvaluator& eval);

    void add_node_operator(std::unique_ptr<NodeOperator> op);
    void add_route_operator(std::unique_ptr<RouteOperator> op);

    // Run all operators to local optimum using granular neighbourhood.
    Solution search(Solution sol);

private:
    const ProblemData& data_;
    const CostEvaluator& eval_;
    std::vector<std::unique_ptr<NodeOperator>> node_ops_;
    std::vector<std::unique_ptr<RouteOperator>> route_ops_;
};
```

---

## 4. Search Algorithms

### 4.1 Iterated Local Search

```cpp
class IteratedLocalSearch {
public:
    IteratedLocalSearch(const ProblemData& data,
                        LocalSearch& ls,
                        RuinAndRecreate& perturb,
                        PenaltyManager& penalty,
                        ILSParams params);

    Solution run(Solution initial, StopCriterion& stop);
};
```

### 4.2 Genetic Algorithm (HGS pattern)

```cpp
class GeneticAlgorithm {
public:
    GeneticAlgorithm(const ProblemData& data,
                     LocalSearch& ls,
                     Population& pop,
                     Crossover& cx,
                     PenaltyManager& penalty,
                     GAParams params);

    Solution run(std::vector<Solution> initial, StopCriterion& stop);
};
```

Both use the same `LocalSearch` engine. No shared base class.

---

## 5. Extensibility Scenarios

### 5.1 Adding a new constraint (e.g., max route duration)

1. Define `DistanceSegment` with `merge()` and `excess_distance(int max_dist)`
2. Add `dist_penalty` to `CostEvaluator`
3. Store prefix distance segments in `Route`
4. **Zero operator changes** — operators use CostEvaluator, which now includes
   the new penalty term automatically

### 5.2 Adding a new operator (e.g., LLM-discovered)

1. Implement `NodeOperator` or `RouteOperator` subclass
2. Or provide a `PerturbFn` lambda for destroy/repair
3. Register with `LocalSearch` or `RuinAndRecreate`
4. **Zero segment/constraint changes**

### 5.3 Adding a new algorithm (e.g., neural-guided search)

1. Write a new class that uses `LocalSearch`, `PenaltyManager`, etc.
2. No base class to inherit from — just use the components
3. Can mix with existing components freely

### 5.4 LLM-driven operator evolution

```cpp
// EoH/VRPAgent pattern: LLM generates destroy functions as code
class LLMDestroyOperator {
    std::string source_code;  // the LLM-generated code
    DestroyFn compiled_fn;    // compiled or interpreted

    std::vector<int> operator()(Solution& sol, int count, RNG& rng) {
        return compiled_fn(sol, count, rng);
    }
};

// Evolutionary loop over a population of LLM-generated operators
class OperatorEvolver {
    std::vector<LLMDestroyOperator> population;
    void evaluate_on_instances(const std::vector<ProblemData>& instances);
    void evolve(LLM& llm);  // ask LLM to mutate/crossover operator code
};
```

The framework's clean operator interface (`DestroyFn`, `RepairFn`, `PerturbFn`)
makes it natural to plug in LLM-generated functions.

### 5.5 Scheduling — different problem, shared search layer

```cpp
// Scheduling has its own concrete types
class ScheduleLocalSearch {
    void add_operator(std::unique_ptr<ScheduleOperator> op);
    Schedule search(Schedule s);
};

// But reuses the same search controllers
class IteratedLocalSearch<Schedule, ScheduleLocalSearch, SchedulePerturbation> { ... };
```

Or more likely: scheduling gets its own ILS class that composes scheduling
components. The search *patterns* are shared (acceptance criteria, stopping,
population management), even if the classes aren't literally the same.

---

## 6. Implementation Phases

### Phase 1 — CVRP Core

The LS engine. 80% of the value.

1. Repo skeleton (CMake, C++23, Catch2)
2. ProblemData + CVRPLIB parser
3. LoadSegment with merge()
4. Route with prefix segment arrays
5. Solution + CostEvaluator
6. Operators: Relocate(1,0), Swap(1,1), 2-opt, Or-opt
7. LocalSearch engine (granular neighbourhood)
8. Construction heuristic (nearest-neighbour)
9. ILS (ruin-and-recreate + late acceptance)
10. Tests: operator correctness, small instance end-to-end

### Phase 2 — Benchmark Quality

1. SWAP* operator
2. PenaltyManager (adaptive α)
3. Benchmark harness on Uchoa X-set, report gaps to BKS
4. Tuning. Target: <2% average gap, 60s.

### Phase 3 — HGS

1. Population (feasible + infeasible subpops, broken pairs diversity)
2. Crossover (SREX)
3. GeneticAlgorithm class
4. Compare ILS vs HGS on X-set

### Phase 4 — VRPTW

1. DurationSegment with merge()
2. Update Route to carry duration segments
3. PenaltyManager adds β (time warp)
4. Operators: zero changes (they use CostEvaluator)
5. Solomon + Gehring-Homberger benchmarks

### Phase 5 — Scheduling

New problem family. Separate concrete types. Search patterns carry over.

### Phase 6 — Lot Sizing

Fix-and-Optimize with mip-heuristics as inner solver.

---

## 7. Relationship to mip-heuristics

```
mip-heuristics (sibling repo)         primal-rsp (this repo)
┌──────────────────────────┐          ┌──────────────────────────┐
│ Generic MIP              │          │ Structured problems      │
│                          │          │                          │
│ • FJ solver              │◄─────────│ • Fix-and-Optimize uses  │
│ • Local-MIP solver       │ Phase 6  │   FJ/Local-MIP as inner  │
│ • MPS reader             │          │   subproblem solver      │
│ • Presolve               │          │                          │
│                          │          │ • Routing: CVRP, VRPTW   │
│ Vars: generic x_i        │          │ • Scheduling: JSP, FSP   │
│ Constraints: Ax ≤ b      │          │ • Lot sizing: CLSP       │
│ Neighborhoods: ±1 per var│          │                          │
│                          │          │ Neighborhoods: relocate,  │
│                          │          │   swap, 2-opt, SWAP*, N5 │
└──────────────────────────┘          └──────────────────────────┘
```

---

## 8. Key Design Decisions

1. **Integer distances** — scale and round. CVRPLIB uses integers. Avoids
   floating-point in the hot loop. Cache-friendly.

2. **Granular neighbourhood** — only consider k-nearest (k=40). O(n·k) not O(n²).

3. **Segment concatenation** — the extensibility mechanism. New constraint =
   new segment type with merge(). Operators and algorithms untouched.

4. **Penalized cost** — `distance + α·excessLoad + β·timeWarp + ...`. Infeasible
   intermediate solutions allowed. Adaptive weights.

5. **Operator ordering** — cheapest-first: Relocate, Swap, 2-opt, Or-opt, SWAP*.

6. **No ALNS** — ILS and HGS both outperform it. ALNS's adaptive operator
   selection adds complexity without matching purpose-built search quality.

7. **No abstract Algorithm base** — ILS and HGS are separate classes. A custom
   algorithm just uses the components directly.

8. **Clean function signatures for LLM targeting** — `DestroyFn`, `RepairFn`,
   `PerturbFn` are simple enough for LLM code generation.

---

## 9. References

Core methods:
- Vidal et al. (2014). *A unified solution framework for multi-attribute VRPs*. C&OR.
- Vidal (2022). *HGS for the CVRP: SWAP\**. C&OR.
- Wouda et al. (2024). *PyVRP: a high-performance VRP solver package*. IJOC.
- Máximo et al. (2024). *AILS-II: Adaptive ILS for large-scale CVRP*. IJOC.
- Accorsi & Vigo (2021). *FILO: Fast and scalable heuristic for large-scale CVRP*. TS.

Benchmarks:
- Uchoa et al. (2017). *New benchmark instances for the CVRP*. EJOR.
- CVRPLIB BKS Challenge (2026). https://vrp.galgos.inf.puc-rio.br/

Scheduling:
- Nowicki & Smutnicki (1996). *A fast taboo search for the job shop problem*. MS.

Production planning:
- Helber & Sahling (2010). *Fix-and-optimize for multi-level CLSP*. IJPE.
- Muller, Spoorendonk & Pisinger (2012). *Hybrid ALNS for lot-sizing*. EJOR.

LLM/Neural:
- Ye et al. (2025). *VRPAgent: LLM-driven operator discovery for VRP*. arXiv.
- Liu et al. (2024). *Evolution of Heuristics (EoH)*. ICML.
- Romera-Paredes et al. (2024). *FunSearch*. Nature.
