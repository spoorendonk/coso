# Routing & Scheduling Heuristics — Problem Landscape and Framework Design

Research notes for extending the Flowty LP-Free Heuristic Solver with routing and scheduling capabilities.

---

## 1. Are Routing and Scheduling One Framework or Two?

**Short answer: they share enough structure to live in one framework, but they are distinct problem families with different core representations and neighborhoods.**

### What they share

| Aspect | Routing | Scheduling |
|--------|---------|------------|
| Complexity class | NP-hard | NP-hard |
| Decision variables | Sequencing + assignment | Sequencing + assignment |
| Constraint style | Capacity, time windows, precedence | Precedence, resource capacity, release/due dates |
| Objective flavour | Minimize cost/distance | Minimize makespan/tardiness/cost |
| Solution method | Local search / metaheuristics | Local search / metaheuristics |

Both families decompose into **assignment** decisions (which jobs/customers go where) and **sequencing** decisions (in what order). Both use penalty-based feasibility handling, and both benefit from the same metaheuristic shells (ILS, ALNS, GA+LS hybrids).

### Where they diverge

| Dimension | Routing | Scheduling |
|-----------|---------|------------|
| Core structure | Graph / tour / path | Gantt chart / disjunctive graph |
| Neighborhoods | 2-opt, Or-opt, relocate, swap, SWAP*, cross | Critical-path swaps (N1–N7), block moves |
| Feasibility check | Capacity + time window propagation along route | Precedence + resource contention along critical path |
| Evaluation | Route cost = sum of arc costs + service times | Makespan = length of critical path in disjunctive graph |
| Constraint propagation | Forward along each route | Forward + backward through operation chains |

### Recommendation for the framework

Treat them as **two problem categories under one algorithmic shell**:

```
Algorithm Layer (ILS, ALNS, HGS, SA, LNS)     ← shared
         │
    ┌────┴─────┐
    │          │
 Routing    Scheduling
 Problem    Problem
    │          │
 Graph IR   Disjunctive
 + Routes   Graph IR
    │       + Gantt
    │          │
 Route       Critical-path
 neighborhoods  neighborhoods
```

The ROAR-style algorithm layer in the existing plan already supports this — `Problem` is abstract, and the algorithm just calls `evaluate`, `perturb`, `localSearch`. The problem-specific logic (representation, neighborhoods, incremental evaluation) lives inside the concrete `Problem` subclass.

---

## 2. Well-Known Problem Definitions and Classification

### 2.1 Routing Problems

The Vehicle Routing Problem (VRP) family traces back to Dantzig & Ramser (1959). The canonical taxonomy by Eksioglu et al. (2009) and updates by Braekers et al. (2016) classify VRP variants along several axes:

**Core variants:**

| Problem | Key constraint | Notation |
|---------|---------------|----------|
| **TSP** | Visit all nodes, single tour | — |
| **CVRP** | Vehicle capacity | C |
| **VRPTW** | Time windows at each customer | TW |
| **VRPPD / PDP** | Pickup and delivery pairs | PD |
| **MDVRP** | Multiple depots | MD |
| **OVRP** | Vehicles don't return to depot | O |
| **SDVRP** | Split deliveries allowed | SD |
| **PVRP** | Periodic visits over planning horizon | P |
| **DVRP** | Dynamic customer arrivals | D |
| **TDVRP** | Time-dependent travel times | TD |
| **GVRP / EVRP** | Green / electric vehicle constraints | G/E |
| **HFVRP** | Heterogeneous fleet | HF |

**Classification dimensions** (following Braekers et al. 2016):
- Customer-related: time windows, service times, demands, priorities
- Vehicle-related: capacity, fleet size, heterogeneity, max route duration
- Depot-related: single/multiple, open/closed routes

### 2.2 Scheduling Problems

Scheduling uses the three-field notation **α | β | γ** (Graham et al. 1979):

- **α** — Machine environment
- **β** — Job/processing characteristics
- **γ** — Objective function

**Machine environments (α):**

| Code | Problem | Description |
|------|---------|-------------|
| 1 | Single machine | One machine, n jobs |
| P | Parallel machines | m identical machines |
| Q | Uniform parallel | Machines with different speeds |
| R | Unrelated parallel | Machine-job dependent processing times |
| F | Flow shop | Jobs pass through machines in same order |
| J | Job shop | Each job has its own machine order |
| O | Open shop | No precedence on machine order |
| FJ | Flexible job shop | Job shop + machine flexibility |

**Job characteristics (β):**
- `prec` — precedence constraints
- `rj` — release dates
- `dj` — due dates
- `pij` — machine-dependent processing times
- `sjk` — sequence-dependent setup times
- `batch` — batching constraints
- `no-wait` — no waiting between operations

**Objectives (γ):**
- `Cmax` — makespan (most studied)
- `ΣCj` — total completion time
- `ΣwjTj` — total weighted tardiness
- `Lmax` — maximum lateness
- `ΣUj` — number of tardy jobs

### 2.3 Where Does Production Planning Fit?

Production planning sits at a **higher level of abstraction** than shop scheduling:

| Level | Scope | Horizon | Decisions |
|-------|-------|---------|-----------|
| **Strategic planning** | Facility location, capacity | Years | Where to build, how much capacity |
| **Production planning** | Lot sizing, MRP | Weeks–months | How much to produce each period |
| **Scheduling** | Job/operation sequencing | Hours–days | In what order, on which machine |
| **Routing** | Vehicle dispatching | Hours–days | Which vehicle visits which customer |

Production planning (lot sizing, material requirements planning) is typically modeled as MIP with inventory balance constraints and solved with MIP solvers or decomposition. It **connects to** scheduling and routing — the output of planning feeds the input of scheduling and routing — but the problem structure is different:

- **Lot sizing**: multi-period inventory balance + setup costs → MIP with network flow structure
- **Scheduling**: sequencing + resource assignment → disjunctive/conjunctive graph
- **Routing**: tour construction + capacity → graph/TSP structure

For the framework, production planning is best treated as a **generic MIP** problem (already covered by the existing plan), while scheduling and routing get specialized representations and neighborhoods.

---

## 3. State of the Art: Local Search Heuristics That Work

### 3.1 For Routing (CVRP and variants)

The current leaderboard is clear. The **CVRPLIB BKS Challenge** (Jan–Feb 2026) on the XL instances (1,000–10,000 customers) from Uchoa et al. established the ranking definitively:

#### Tier 1: Current state of the art

**AILS-II** — Adaptive Iterated Local Search (Máximo, Cordeau & Nascimento, 2024, INFORMS J. Computing)
- Won the 2026 BKS Challenge decisively, holding 93/100 initial BKS values on XL instances
- Two-phase ILS: Phase 1 uses acceptance criterion for reference solution; Phase 2 selects from elite pool
- Adaptive perturbation degree and acceptance criterion (diversity control)
- Standard LS neighborhoods: relocate, swap, 2-opt, Or-opt
- Perturbation via ruin-and-recreate (removal heuristics)
- Scales to 30,000 customers
- Mean gap: 0.07% to BKS on XL instances

**HGS-CVRP** — Hybrid Genetic Search (Vidal, 2022, Computers & OR)
- Dominant on medium-scale instances (100–1,000 customers)
- Genetic algorithm shell + local search education of offspring
- Key innovation: **SWAP\* neighborhood** — exchange two customers between routes without insertion-in-place, pruned by geometric arguments
- LS operators applied in order of increasing complexity
- Linear-time optimal Split algorithm
- Open source: github.com/vidalt/HGS-CVRP

#### Tier 2: Very competitive

**FILO / FILO2** — Fast Iterated Localized Optimization (Accorsi & Vigo, 2021/2024)
- ILS + simulated annealing acceptance
- "Localized" optimization: keeps search geometrically local
- Multiple neighborhood operators + ruin-and-recreate
- Highly scalable, open source: github.com/acco93/filo

**SISRs** — Slack Induction by String Removals (Christiaens & Vanden Berghe, 2020)
- Ruin-and-recreate with string removal perturbation
- Simple but effective, especially for VRPTW

**ALNS** — Adaptive Large Neighborhood Search (Ropke & Pisinger, 2006)
- Framework using multiple destroy/repair operators with adaptive selection
- Sequence-based removal operators most effective
- Regret insertion operators best for repair
- Excellent generality: works across CVRP, VRPTW, PDP, MDVRP, etc.

#### Core local search operators (shared across methods)

**Intra-route (sequencing):**

| Operator | Description | Complexity |
|----------|-------------|------------|
| 2-opt | Reverse a sub-path | O(n²) per route |
| Or-opt(k) | Move segment of k customers within route | O(n²) per route |
| 3-opt | Three-edge exchange | O(n³), rarely needed |

**Inter-route (assignment + sequencing):**

| Operator | Description |
|----------|-------------|
| Relocate(1,0) | Move 1 customer to another route |
| Relocate(2,0) | Move 2 consecutive customers |
| Swap(1,1) | Exchange 1 customer between two routes |
| Swap(2,1) | Exchange 2 vs 1 customers |
| Swap(2,2) | Exchange 2 vs 2 customers |
| SWAP* | Exchange 2 customers without in-place insertion (Vidal) |
| 2-opt* / Cross | Swap tails of two routes |
| Ejection chains | Chain of relocations across multiple routes |

**Key insight from the literature**: simple neighborhoods (relocate, swap, 2-opt) are sufficient for near-optimal intra-route solutions because CVRP routes are typically short (15–20 customers). The quality gains come from **inter-route moves** and the **metaheuristic shell** (perturbation + acceptance).

### 3.2 For Scheduling (Job Shop and variants)

#### Best-performing approaches

**Tabu Search with critical-path neighborhoods** remains the strongest single-method approach:
- N5 neighborhood (Nowicki & Smutnicki, 1996) and successors
- Block-based moves on the critical path of the disjunctive graph
- Fast incremental makespan evaluation

**Hybrid GA + LS**:
- Genetic algorithm for diversification
- Tabu search or SA as local search for intensification
- Similar pattern to HGS for routing

**Key scheduling neighborhoods:**

| Operator | Description |
|----------|-------------|
| Adjacent swap (N1) | Swap two adjacent operations on a machine |
| Block swap (N5) | Swap operations at block boundaries on critical path |
| Critical arc reversal | Reverse a disjunctive arc on the critical path |
| Insertion move | Remove operation, reinsert at different position on machine |
| Assignment move (FJSP) | Change machine assignment for an operation |

### 3.3 Common Metaheuristic Shells (work for both)

| Shell | Key idea | Best for |
|-------|----------|----------|
| **ILS** | Perturb + local search + acceptance | CVRP (AILS-II), JSP |
| **ALNS** | Multiple destroy/repair + adaptive selection | VRP variants, flexible |
| **HGS** | GA + educated local search + population diversity | CVRP (medium scale), VRPTW |
| **SA** | Temperature-based acceptance of worse solutions | JSP, CVRP (within FILO) |
| **LNS** | Destroy large part of solution + reconstruct | VRP, scheduling |
| **VNS** | Systematic neighborhood switching | Both |

---

## 4. The CVRPLIB BKS Challenge (Uchoa et al.)

### 4.1 Background: Uchoa et al. (2017) X-Set

Uchoa, Pecin, Pessoa, Poggi, Vidal & Subramanian published "New benchmark instances for the Capacitated Vehicle Routing Problem" (European J. OR, 257(3):845–858, 2017). This created the **X-Set**: 100 instances, 100–1,000 customers, designed to be heterogeneous and challenging. It replaced older benchmarks that had become too easy or too artificial. As of today, 61/100 X instances have proven optimal solutions.

### 4.2 XL Extension (2025–2026)

A decade later, the benchmark was extended with **100 XL instances** (1,000–10,000 customers). This motivated the **CVRPLIB BKS Challenge**:

- **Duration**: Jan 12 – Feb 11, 2026 (30 days)
- **Format**: Submit improved feasible solutions, auto-verified, live leaderboard
- **Goal**: Establish high-quality BKS for XL instances
- **Participants**: Classic OR, ML-based, and hybrid approaches

### 4.3 Results

**AILS-II dominated**: held 93/100 initial BKS values, mean gap 0.07%.

Runner-up methods:
- **FILO2**: 6/100 BKS values, mean gap 0.21%
- **FILO**: 1/100 BKS values, mean gap 0.25%
- **KGLS-XXL**: mean gap 1.00%

The challenge confirmed that **iterated local search with adaptive perturbation** (AILS-II) is the current state of the art for large-scale CVRP, while **hybrid genetic search** (HGS) remains dominant at medium scale.

ML-based methods have not yet matched classical OR heuristics on deterministic CVRP, though hybrid ML+OR approaches are closing the gap.

---

## 5. Emerging Directions: Neural and LLM-Based Approaches

Worth tracking but not yet ready for production use.

### 5.1 Neural Combinatorial Optimization (NCO)

RL-trained neural solvers (attention/transformer models) that construct or
improve solutions directly:

| Scale | NCO vs Classical | Status |
|-------|-----------------|--------|
| Small (≤100 nodes) | <1% gap to HGS, much faster inference | Competitive |
| Medium (100–1K) | HGS still dominates with enough compute | Not competitive |
| Large (1K–100K) | GLOP, SIL outperform HGS in speed+quality | Promising |
| Very large (1M+) | L2R is the only method that can even run | Only option |

Key methods: POMO, GLOP (divide-and-conquer), SIL (self-improved learning),
L2R (learn to reduce). Weakness: poor generalization across distributions and
constraint types, still lags on medium-scale deterministic instances.

### 5.2 LLM-Driven Heuristic Discovery

LLMs used to *design* heuristic operators rather than solve instances directly:

- **FunSearch** (DeepMind, 2024): evolutionary search in program space using LLM
  as mutation operator. Found new bin-packing heuristics beating known baselines.
- **EoH** (Evolution of Heuristics, 2024): evolves both natural-language "thoughts"
  and executable code. Outperforms FunSearch on TSP, CVRP, bin packing.
- **VRPAgent** (2025): first LLM-based method to advance state-of-the-art on VRPs.
  Discovers novel destroy/repair operators for ALNS-style search.
- **EoH-S** (2025): evolves a *set* of complementary heuristics rather than a
  single best, addressing generalization across diverse instances.

This direction is promising for **automated operator design** — the framework
should make it easy to plug in new operators, which aligns with the component-
based architecture below.

---

## 6. Implications for the Framework

### 6.1 Why not ROAR (abstract Problem/Algorithm)?

The initial plan proposed a ROAR-style abstract `Problem`/`Algorithm` interface.
Looking at what the actual top solvers do, **none of them use this pattern**:

- **HGS-CVRP** (Vidal): explicit design goal is "stay simple, stand-alone, and
  specialized." No abstract Problem. No abstract Algorithm.
- **PyVRP**: composable components but no abstract Problem/Algorithm layer.
- **AILS-II**: purpose-built, no abstract framework.

The ROAR abstraction hurts for three reasons:

1. **Wrong abstraction boundary** — `localSearch()` and `perturb()` are algorithmic
   concerns, not problem concerns. The problem is "CVRP with 1000 customers."
   The search strategy is a separate decision.

2. **Virtual dispatch on the hot path** — `evaluate()` gets called millions of
   times. Routing and scheduling need completely different incremental evaluation.
   Making this virtual adds overhead for no benefit.

3. **False genericity** — ILS and HGS have fundamentally different structures
   (single solution vs population, perturbation vs crossover). Forcing them into
   the same `Algorithm::run()` interface means the interface is either too vague
   to be useful or too specific to be general.

### 6.2 What actually works: component composition (PyVRP pattern)

PyVRP demonstrates the right architecture. Share **components**, not interfaces:

```cpp
// 1. Operators are the abstraction (the reusable unit)
class NodeOperator {
    virtual Cost evaluate(Node u, Node v, Solution&) = 0;
};
class RouteOperator {
    virtual Cost evaluate(Route& r1, Route& r2, Solution&) = 0;
};
// Same relocate/swap works whether called from ILS, HGS, or SA

// 2. Local search engine composes operators (shared by all algorithms)
class LocalSearch {
    void addNodeOperator(NodeOperator* op);
    void addRouteOperator(RouteOperator* op);
    Solution search(Solution& s);  // run all operators to local optimum
};

// 3. Each algorithm is its own class — no forced common interface
class GeneticAlgorithm {
    GeneticAlgorithm(ProblemData& data, LocalSearch& ls,
                     Population& pop, Crossover& cx);
    Result run(StopCriterion& stop);
};

class IteratedLocalSearch {
    IteratedLocalSearch(ProblemData& data, LocalSearch& ls,
                        Perturbation& perturb, AcceptanceCriterion& accept);
    Result run(StopCriterion& stop);
};

// 4. Utility components
class PenaltyManager { ... };    // adaptive penalty weights
class Population { ... };        // feasible + infeasible subpops, diversity
class StopCriterion { ... };     // time, iterations, no-improvement
```

Key differences from ROAR:
- **Operators are the abstraction**, not Problem/Algorithm
- **LocalSearch is a concrete shared component**, not a virtual method on Problem
- **Each algorithm is its own class** with its own structure
- **ProblemData is concrete** — distances, demands, time windows — not behind `evaluate()`

### 6.3 Problem-type specialization

Each problem family provides its own concrete types:

| Component | Routing | Scheduling | Lot Sizing |
|-----------|---------|------------|------------|
| Solution | Routes (customer sequences) | Schedule (op sequences per machine) | Production plan (setups + quantities) |
| ProblemData | Distance matrix, demands, TWs | Processing times, precedence | Periods, demands, setup costs |
| NodeOperators | Relocate, Swap, SWAP* | N5 block swap, insertion | — |
| RouteOperators | 2-opt*, Cross, SWAP* | — | — |
| Perturbation | Ruin-and-recreate | Random critical-path swaps | Fix-and-Optimize (MIP subproblem) |
| Crossover | OX, SREX | POX, JBX | Period-based crossover |
| Evaluation | Sum of route costs | Critical path length | Setup + holding cost |

### 6.4 Production planning: MIP-based neighborhoods

Production planning (lot sizing) differs from routing and scheduling in that the
best local search approaches use **MIP subproblems as neighborhoods**:

**Fix-and-Optimize (FO)** is the dominant paradigm:
1. Fix most binary (setup) variables to their current values
2. Free a subset along one decomposition dimension
3. Re-optimize that subproblem with a MIP solver
4. Repeat with different subsets

| Decomposition | What gets re-optimized |
|---|---|
| Product | All periods for a subset of products |
| Time period | All products for a window of periods |
| Resource | All products sharing a machine/resource |

The state of the art combines **Relax-and-Fix** (construction) + **Fix-and-Optimize**
(improvement) + **VNS** (diversification), achieving gaps under 1% from optimum.

### 6.5 Priority for implementation

1. **CVRP data structures** — Solution, ProblemData, Route, distance matrix
2. **LS operators** — relocate, swap, 2-opt, Or-opt, SWAP*
3. **LocalSearch engine** — compose operators, run to local optimum
4. **ILS** — perturbation (ruin-and-recreate) + acceptance + penalty management
5. **HGS** — population + crossover + education (reuses same LS engine)
6. **Benchmark on Uchoa X instances** — proves quality
7. **Extend to VRPTW** — add time window propagation to operators
8. **Scheduling** — new Solution/ProblemData types, critical-path operators

### 6.6 Key design decisions

1. **Route representation**: ordered customer sequence per route (not edge
   variables). This is what all top methods use.

2. **Incremental evaluation**: precompute cumulative load, distance, and time
   along each route. Delta evaluation for moves should be O(1) for
   relocate/swap, O(route length) for 2-opt.

3. **Neighbor lists**: precompute k-nearest neighbors per customer (typically
   k=20–40) to prune the neighborhood search. Critical for scaling.

4. **Penalized cost**: `cost + α*capacityViolation + β*TWViolation` as the LS
   objective, with adaptive penalty weights. Allows infeasible intermediate
   solutions (as in HGS and AILS-II).

5. **Perturbation**: ruin-and-recreate is the dominant strategy. Remove a segment
   of customers (random, worst, related, string removal), then reinsert with
   greedy/regret heuristic.

---

## 7. Key References

- Uchoa et al. (2017). *New benchmark instances for the CVRP*. European J. OR 257(3):845–858.
- Vidal (2022). *Hybrid genetic search for the CVRP: Open-source implementation and SWAP\* neighborhood*. Computers & OR 140:105643.
- Máximo, Cordeau & Nascimento (2024). *AILS-II: An Adaptive Iterated Local Search Heuristic for the Large-Scale CVRP*. INFORMS J. Computing 36(4):974–986.
- Accorsi & Vigo (2021). *A Fast and Scalable Heuristic for the Solution of Large-Scale CVRP*. Transportation Science 55(4):832–856.
- Ropke & Pisinger (2006). *An Adaptive Large Neighborhood Search Heuristic for the PDP with Time Windows*. Transportation Science 40(4):455–472.
- Eksioglu et al. (2009). *The vehicle routing problem: A taxonomic review*. Computers & Industrial Engineering 57(4):1472–1483.
- Braekers et al. (2016). *The VRP: State of the art classification and review*. Computers & Industrial Engineering 99:300–313.
- Graham et al. (1979). *Optimization and approximation in deterministic sequencing and scheduling*. Annals of Discrete Mathematics 5:287–326.
- Nowicki & Smutnicki (1996). *A fast taboo search algorithm for the job shop problem*. Management Science 42(6):797–813.
- CVRPLIB BKS Challenge (2026). https://vrp.galgos.inf.puc-rio.br/index.php/en/bks-challenge
- Helber & Sahling (2010). *A fix-and-optimize approach for the multi-level capacitated lot sizing problem*. Int. J. Production Economics 123(2):247–256.
- Seeanner et al. (2013). *Combining VNDS with Fix&Optimize for multi-level lot-sizing and scheduling*. Computers & OR 40(9):2110–2124.
- Chen et al. (2015). *Fix-and-optimize and VNS for multi-level capacitated lot sizing*. Omega 56:25–36.
- Muller, Spoorendonk & Pisinger (2012). *A hybrid adaptive large neighborhood search heuristic for lot-sizing with setup times*. European J. OR 218(3):614–633.
- Wouda et al. (2024). *PyVRP: a high-performance VRP solver package*. INFORMS J. Computing 36(3):615–629.
- Romera-Paredes et al. (2024). *Mathematical discoveries from program search with large language models*. Nature 625:468–475. (FunSearch)
- Liu et al. (2024). *Evolution of Heuristics: Towards Efficient Automatic Algorithm Design Using LLM*. ICML 2024.
- Ye et al. (2025). *VRPAgent: LLM-Driven Discovery of Heuristic Operators for Vehicle Routing Problems*. arXiv:2510.07073.
