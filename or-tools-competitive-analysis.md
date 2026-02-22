# Google OR-Tools — Competitive Analysis for primal-rsp

Thorough analysis of Google OR-Tools features and concepts, assessed against our
planned primal-rsp library covering routing, scheduling, assignment/timetabling,
packing, production planning, and network flow.

---

## 1. OR-Tools Routing Library (RoutingModel)

The routing library is OR-Tools' most mature and widely-used component. It is
built on top of their original constraint programming solver (not CP-SAT), uses
a RoutingModel class where the user builds a model, adds dimensions and
constraints, then solves with configurable search parameters.

### 1.1 Dimensions (Cumulative Tracking Along Routes)

OR-Tools' core modeling concept is the **Dimension** — a mechanism that tracks
quantities accumulating along a vehicle's route (load, time, distance, etc.).
Each dimension has:

- **Transit variables**: increase/decrease of quantity at each step (from callback)
- **Cumulative variables**: total accumulated at each node (CumulVar)
- **Slack variables**: waiting time at nodes (`slack(i) = cumul(j) - cumul(i) - transit(i,j)`)

**Dimension creation variants:**

| Method | Description | Assessment |
|--------|-------------|------------|
| `AddDimension(callback, slack_max, capacity, fix_start)` | Basic dimension | **(a) Have** — our Resource concept |
| `AddDimensionWithVehicleCapacity(callback, slack, caps[], ...)` | Per-vehicle capacity | **(a) Have** — heterogeneous fleet |
| `AddDimensionWithVehicleTransits(callbacks[], ...)` | Per-vehicle transit functions | **(a) Have** — routing profiles |
| `AddDimensionWithVehicleTransitAndCapacity(...)` | Both per-vehicle | **(a) Have** |
| `AddConstantDimension(value, capacity, ...)` | Fixed transit value | **(a) Have** — trivial resource case |
| `AddVectorDimension(values[], capacity, ...)` | Node-dependent transit | **(a) Have** |
| `AddMatrixDimension(matrix, capacity, ...)` | Arc-dependent transit matrix | **(a) Have** — standard distance matrix |
| `AddDimensionDependentDimensionWithVehicleCapacity(...)` | Dimension depends on another dimension | **(b) Relevant** — e.g., fuel consumption depends on load weight |
| `AddCumulDependentTransitCallback()` | Transit depends on cumulative value | **(b) Relevant** — e.g., speed depends on accumulated time |
| `RegisterStateDependentTransitCallback()` | Complex state-dependent transits | **(b) Relevant** — advanced time-dependent routing |

**Assessment:** Our Resource abstraction (init/merge/merge_reverse/excess) maps
directly to their Dimension concept. We have the equivalent for load, duration,
distance. The **dimension-dependent-dimension** and **cumul-dependent transit**
features are notable gaps — these handle cases where one quantity depends on
another (fuel consumption varying with load, speed depending on time of day).
We should add these.

### 1.2 Routing Constraints

| Constraint | OR-Tools Method | Assessment |
|------------|----------------|------------|
| Pickup-delivery pairing | `AddPickupAndDelivery()` | **(a) Have** — PrecedenceResource |
| Pickup-delivery sets | `AddPickupAndDeliverySets()` | **(a) Have** — client groups |
| PD ordering policy (LIFO/FIFO/none) | `SetPickupAndDeliveryPolicyOfVehicle()` | **(b) Relevant** — we have precedence but not LIFO/FIFO stack constraints |
| Disjunction (optional nodes + penalty) | `AddDisjunction()` | **(a) Have** — optional clients with prize/penalty |
| Disjunction max cardinality | `AddDisjunction(indices, penalty, max_cardinality)` | **(a) Have** — client groups (exactly one from group) |
| Penalty cost behavior (once vs per-inactive) | `PENALIZE_ONCE` / `PENALIZE_PER_INACTIVE` | **(b) Relevant** — useful for group penalties |
| Vehicle allowed for node | `SetAllowedVehiclesForIndex()` | **(a) Have** — site-dependent VRP |
| Soft same-vehicle | `AddSoftSameVehicleConstraint()` | **(b) Relevant** — penalty for splitting a group across vehicles |
| Max active vehicles | `SetMaximumNumberOfActiveVehicles()` | **(a) Have** — fleet size constraint |
| Vehicle used when empty | `SetVehicleUsedWhenEmpty()` | **(c) Not relevant** — edge case |
| Visit types with policies | `SetVisitType()` with ADD/REMOVE policies | **(b) Relevant** — models LIFO/FIFO stack loading |
| Hard type incompatibility | `AddHardTypeIncompatibility()` | **(b) Relevant** — certain cargo types cannot share a vehicle |
| Temporal type incompatibility | `AddTemporalTypeIncompatibility()` | **(b) Relevant** — types cannot be simultaneously active on vehicle |
| Type requirements (same vehicle) | `AddSameVehicleRequiredTypeAlternatives()` | **(b) Relevant** — "if carrying hazmat, must also carry safety kit" |
| Type requirements (when adding) | `AddRequiredTypeAlternativesWhenAddingType()` | **(b) Relevant** |
| Resource groups | `AddResourceGroup()` | **(b) Relevant** — depot loading docks, shared equipment |
| Route constraint (custom) | `AddRouteConstraint()` | **(a) Have** — move filters + custom resources |
| Break scheduling | `BreakPropagator`, interbreak rules | **(b) Relevant** — driver breaks with min duration + max interbreak time |

### 1.3 Cost Model

| Feature | OR-Tools | Assessment |
|---------|----------|------------|
| Arc cost evaluator per vehicle | `SetArcCostEvaluatorOfVehicle()` | **(a) Have** — unit_distance_cost per vehicle type |
| Fixed cost per vehicle | `SetFixedCostOfVehicle()` | **(a) Have** |
| Amortized cost (linear + quadratic) | `SetAmortizedCostFactorsOfVehicle()` | **(b) Relevant** — density-based cost: penalize routes with few stops |
| Path energy cost | `SetPathEnergyCostOfVehicle()` | **(b) Relevant** — energy = integral of force over distance, for EVRP |
| Path energy with thresholds | `SetPathEnergyCostsOfVehicle()` | **(b) Relevant** — piecewise energy cost with force thresholds |
| Span cost coefficient | On dimensions: minimize span (max - min cumul) | **(b) Relevant** — minimize route duration directly as cost |
| Soft upper/lower bounds on cumul | `SetCumulVarSoftUpperBound()` | **(a) Have** — soft time windows via penalties |
| Global span cost | Minimize max span across all routes | **(b) Relevant** — makespan-like objective for routing |

### 1.4 Search Parameters

#### First Solution Strategies

| Strategy | Description | Assessment |
|----------|-------------|------------|
| PATH_CHEAPEST_ARC | Greedy nearest-neighbor | **(a) Have** — nearest-neighbour construction |
| PATH_MOST_CONSTRAINED_ARC | Most constrained arc first | **(b) Relevant** |
| SAVINGS | Clarke-Wright savings | **(a) Have** |
| SWEEP | Wren-Holliday sweep | **(c) Not relevant** — rarely best |
| CHRISTOFIDES | Christofides variant | **(c) Not relevant** — only for TSP, we use LS |
| BEST_INSERTION | Cheapest node at cheapest position | **(a) Have** — cheapest insertion |
| PARALLEL_CHEAPEST_INSERTION | Parallel version | **(a) Have** — greedy repair |
| SEQUENTIAL_CHEAPEST_INSERTION | Sequential route construction | **(a) Have** |
| LOCAL_CHEAPEST_INSERTION | Farthest nodes first | **(b) Relevant** — farthest-insertion variant |
| LOCAL_CHEAPEST_COST_INSERTION | Uses routing model cost | **(a) Have** |
| GLOBAL_CHEAPEST_ARC | Global greedy | **(a) Have** |
| ALL_UNPERFORMED | All nodes inactive (for optional) | **(a) Have** — trivial for overconstrained |
| FIRST_UNBOUND_MIN_VALUE | Variable assignment heuristic | **(c) Not relevant** — CP-specific |

#### Local Search Metaheuristics

| Metaheuristic | Assessment |
|---------------|------------|
| GREEDY_DESCENT | **(a) Have** — standard local search |
| GUIDED_LOCAL_SEARCH (GLS) | **(b) Relevant** — we have strategic oscillation, GLS is a notable alternative |
| SIMULATED_ANNEALING | **(a) Have** — composable acceptor |
| TABU_SEARCH | **(a) Have** — composable acceptor |
| GENERIC_TABU_SEARCH | **(a) Have** — objective-value tabu |

#### Local Search Operators (40+ in OR-Tools)

**Inter-route:**
| Operator | Assessment |
|----------|------------|
| Relocate | **(a) Have** — Exchange(1,0) |
| RelocatePair | **(a) Have** — pair_operators.h |
| LightRelocatePair (FIFO/LIFO aware) | **(b) Relevant** — FIFO/LIFO-aware pair relocation |
| RelocateNeighbors (chain) | **(b) Relevant** — moves chains of neighboring nodes |
| RelocateSubtrip | **(b) Relevant** — moves PD subtrips maintaining pairing |
| Exchange | **(a) Have** — Exchange(1,1) |
| ExchangePair | **(a) Have** — SwapPairs |
| ExchangeSubtrip | **(b) Relevant** — exchanges PD subtrips between routes |
| Cross (2-opt*) | **(a) Have** — SwapTails |
| RelocateExpensiveChain | **(b) Relevant** — targets most expensive subchains |

**Intra-route:**
| Operator | Assessment |
|----------|------------|
| TwoOpt | **(a) Have** — SwapTails intra-route |
| OrOpt | **(a) Have** — Exchange(2,0), Exchange(3,0) |
| LinKernighan | **(c) Not relevant** — rarely helps on short VRP routes |
| TSPOpt | **(c) Not relevant** — full TSP solve on single routes, expensive |
| TwoOptWithShortestPath | **(c) Not relevant** — DAG alternatives, not standard VRP |

**Inactive node (optional):**
| Operator | Assessment |
|----------|------------|
| MakeActive | **(a) Have** — InsertOptional |
| MakeInactive | **(a) Have** — RemoveOptional |
| SwapActive | **(a) Have** — similar to ReplaceGroup |
| MakePairActive/Inactive | **(a) Have** — pair operators |
| PairNodeSwapActive | **(b) Relevant** — combined insert pair + deactivate single |

**LNS (Large Neighborhood Search):**
| Operator | Assessment |
|----------|------------|
| PathLNS / FullPathLNS | **(a) Have** — ruin-and-recreate |
| TSPLNS | **(c) Not relevant** |
| InactiveLNS | **(a) Have** — ruin-and-recreate for optional nodes |
| GlobalCheapestInsertionPathLNS | **(a) Have** — greedy repair |
| LocalCheapestInsertionPathLNS | **(a) Have** |
| RelocatePathGlobalCheapestInsert | **(a) Have** |
| ExpensiveChainLNS variants | **(b) Relevant** — targeted LNS on expensive chains |
| CloseNodesLNS variants | **(a) Have** — related removal |
| VisitTypesLNS variants | **(b) Relevant** — LNS targeting specific visit types |

**Operator selection:**
| Feature | Assessment |
|---------|------------|
| Multi-armed bandit operator selection | **(b) Relevant** — adaptive operator weights via MAB |
| `ls_operator_neighbors_ratio` | **(a) Have** — granular neighbourhood |
| `multi_armed_bandit_compound_operator_*` | **(b) Relevant** — MAB parameters |

### 1.5 ILS Parameters (Recently Added to OR-Tools)

OR-Tools recently added ILS with ruin-and-recreate, showing convergence with
academic best practice:

| Feature | Assessment |
|---------|------------|
| SpatiallyCloseRoutesRuin | **(a) Have** — related removal |
| RandomWalkRuin | **(a) Have** — random removal variant |
| SISRRuin (String Removals) | **(a) Have** — planned, from Christiaens & Vanden Berghe |
| Recreate via cheapest insertion/savings | **(a) Have** — greedy repair |
| GreedyDescentAcceptance | **(a) Have** |
| SimulatedAnnealingAcceptance | **(a) Have** — composable acceptor |
| LateAcceptanceAcceptance | **(a) Have** — composable acceptor |
| CompositeRuinProcedure (RUN_ALL/RUN_ONE) | **(a) Have** — weighted operator selection |
| AllNodesPerformedAcceptance | **(a) Have** — feasibility criterion |
| MoreNodesPerformedAcceptance | **(a) Have** — overconstrained acceptance |
| AbsencesBasedAcceptance | **(b) Relevant** — accept if unperformed absences lower |

### 1.6 Solution Finalization

| Feature | OR-Tools | Assessment |
|---------|----------|------------|
| `AddVariableMinimizedByFinalizer()` | Optimize secondary variables after main solve | **(b) Relevant** — e.g., minimize departure times after route optimization |
| `AddWeightedVariableMinimizedByFinalizer()` | Prioritized secondary optimization | **(b) Relevant** |
| `AddVariableTargetToFinalizer()` | Push variable close to target | **(b) Relevant** — useful for schedule compaction |

### 1.7 Warm Start & Solution Hints

| Feature | Assessment |
|---------|------------|
| `SolveFromAssignmentWithParameters()` | **(a) Have** — warm start |
| `FastSolveFromAssignmentWithParameters()` | **(a) Have** — greedy descent from initial |
| `SolveWithIteratedLocalSearch()` | **(a) Have** — ILS |
| `SetFirstSolutionHint()` | **(a) Have** — initial solution hint |
| `ApplyLocks()` / `ApplyLocksToAllVehicles()` | **(a) Have** — pinning |
| `ComputeLowerBound()` | **(b) Relevant** — linear assignment lower bound for gap reporting |

### 1.8 Filters (Move Pruning)

| Filter | Assessment |
|--------|------------|
| RouteConstraintFilter | **(a) Have** — move filters |
| MaxActiveVehiclesFilter | **(a) Have** |
| NodeDisjunctionFilter | **(a) Have** |
| PathCumulFilter (dimension costs) | **(a) Have** — resource-based evaluation |
| GlobalLPCumulFilter | **(b) Relevant** — LP relaxation for global constraint checking |
| ResourceAssignmentFilter | **(b) Relevant** — LP-based resource assignment feasibility |
| CPFeasibilityFilter | **(c) Not relevant** — CP-specific |
| PickupDeliveryFilter | **(a) Have** |
| VehicleBreaksFilter | **(b) Relevant** — when we add breaks |
| PathEnergyCostChecker | **(b) Relevant** — energy cost with force thresholds |
| TypeRegulationsFilter | **(b) Relevant** — visit type compatibility |
| ActiveNodeGroupFilter | **(b) Relevant** — group activity consistency |

---

## 2. CP-SAT Solver

CP-SAT is OR-Tools' constraint programming solver using SAT methods. It is
primarily an exact solver (not heuristic), but its modeling concepts are
relevant for understanding what scheduling/assignment features users expect.

### 2.1 Variable Types

| Type | Assessment |
|------|------------|
| Integer variables with domain | **(c) Not relevant** — we use specialized representations |
| Boolean/literal variables | **(c) Not relevant** |
| Interval variables (start, size, end) | **(a) Have** — operations in scheduling model |
| Optional interval variables (with presence literal) | **(a) Have** — optional operations |
| Fixed-size interval variables | **(a) Have** — fixed-duration operations |

### 2.2 Constraint Types

**Boolean:**
| Constraint | Assessment |
|------------|------------|
| BoolOr, BoolAnd, AtMostOne, ExactlyOne, BoolXor | **(c) Not relevant** — CP-specific |
| Implication (a => b) | **(c) Not relevant** |

**Linear:**
| Constraint | Assessment |
|------------|------------|
| Linear equality/inequality | **(c) Not relevant** — we don't do LP |
| AllDifferent | **(a) Have** — implicit in permutation representations |
| Element (array indexing) | **(c) Not relevant** |

**Scheduling:**
| Constraint | Assessment |
|------------|------------|
| NoOverlap (disjunctive) | **(a) Have** — disjunctive graph, one job per machine at a time |
| NoOverlap2D (rectangle packing) | **(b) Relevant** — 2D packing not in our plan |
| Cumulative (resource capacity) | **(a) Have** — RCPSP resource capacity |
| Reservoir (level within bounds, refill/empty events) | **(b) Relevant** — interesting for tank/inventory scheduling |
| Interval variables | **(a) Have** — operations |

**Graph/Routing:**
| Constraint | Assessment |
|------------|------------|
| Circuit (Hamiltonian cycle) | **(c) Not relevant** — CP modeling, we use route representation |
| MultipleCircuit (VRP as circuits) | **(c) Not relevant** |
| Routes | **(c) Not relevant** |

**Sequence/Table:**
| Constraint | Assessment |
|------------|------------|
| Table (allowed tuples) | **(c) Not relevant** |
| ForbiddenAssignments | **(c) Not relevant** |
| Automaton (finite state machine on sequence) | **(b) Relevant** — for pattern constraints in nurse rostering (e.g., "no night-early-night sequence") |
| Inverse | **(c) Not relevant** |

**Channeling:**
| Constraint | Assessment |
|------------|------------|
| Half-reified constraints (OnlyEnforceIf) | **(c) Not relevant** — CP mechanism |
| Conditional constraints via boolean linking | **(c) Not relevant** |

### 2.3 CP-SAT Search Strategies

| Feature | Assessment |
|---------|------------|
| AUTOMATIC_SEARCH | **(c) Not relevant** — SAT-specific |
| PORTFOLIO_SEARCH (multiple strategies in parallel) | **(b) Relevant** — concept of portfolio solving |
| LP_SEARCH (reduced cost branching) | **(c) Not relevant** |
| PSEUDO_COST_SEARCH | **(c) Not relevant** |
| HINT_SEARCH (follow solution hint) | **(a) Have** — warm start |
| Multi-worker parallelism (num_workers) | **(b) Relevant** — parallel search concept |
| Solution pool (retain top-N) | **(a) Have** — population in HGS |
| Enumerate all solutions | **(c) Not relevant** — we find good solutions, not all |
| LNS within SAT | **(c) Not relevant** |
| RINS-LNS (relaxation-induced neighborhoods) | **(c) Not relevant** — requires LP |
| Shared tree search across workers | **(c) Not relevant** |
| Absolute/relative gap limits | **(b) Relevant** — gap-based termination (need lower bound) |

### 2.4 CP-SAT Objective

| Feature | Assessment |
|---------|------------|
| Minimize/Maximize linear expression | **(a) Have** — cost function |
| Float objective (converted to int) | **(c) Not relevant** |
| Multi-objective | **(c) Not relevant** — CP-SAT doesn't natively support it |
| Assumptions (for infeasibility analysis) | **(b) Relevant** — identifying which constraints cause infeasibility |

---

## 3. Network Flow Solvers

### 3.1 Min-Cost Flow (SimpleMinCostFlow)

| Feature | Assessment |
|---------|------------|
| Add arcs with capacity + unit cost | **(a) Have** — NetworkModel |
| Set node supply/demand | **(a) Have** — commodities |
| Solve (balanced supply/demand) | **(a) Have** |
| SolveMaxFlowWithMinCost | **(b) Relevant** — max flow at min cost, useful when supply/demand not balanced |
| Per-arc flow query | **(a) Have** |
| Cost-scaling push-relabel algorithm | **(b) Relevant** — algorithm choice matters; we planned network simplex |
| Negative arc costs | **(a) Have** |
| Self-loops and duplicate arcs | **(a) Have** |
| Infeasibility detection | **(a) Have** |

### 3.2 Max Flow (SimpleMaxFlow)

| Feature | Assessment |
|---------|------------|
| Add arcs with capacity | **(c) Not relevant** — pure max flow is not in our scope |
| Source-sink max flow | **(c) Not relevant** |

**Assessment:** Pure max flow is a subroutine, not a problem we solve directly.
If needed for feasibility checking, we can use a third-party implementation.

### 3.3 Linear Sum Assignment

| Feature | Assessment |
|---------|------------|
| Cost-scaling push-relabel for bipartite matching | **(b) Relevant** — useful as construction heuristic for assignment problems |
| Optimal bipartite assignment | **(b) Relevant** — lower bound computation |
| Infeasibility detection | **(b) Relevant** |

---

## 4. Graph Algorithms

| Algorithm | OR-Tools Implementation | Assessment |
|-----------|------------------------|------------|
| Bounded Dijkstra (single/multi source) | BoundedDijkstraWrapper | **(b) Relevant** — for RCSPP pricing in column generation |
| K-shortest paths (Yen's) | YenKShortestPaths | **(b) Relevant** — for RCMCF alternatives |
| Minimum spanning tree (Kruskal + Prim) | BuildKruskalMST, BuildPrimMST | **(b) Relevant** — lower bounds, construction heuristics |
| Hamiltonian path (Held-Karp DP) | HamiltonianPathSolver | **(c) Not relevant** — O(n^2 * 2^n), only for tiny TSP |
| Eulerian path (Hierholzer) | BuildEulerianTour/Path | **(c) Not relevant** — not used in VRP/scheduling |
| Connected components | ConnectedComponents | **(c) Not relevant** |
| Strongly connected components | StronglyConnectedComponents | **(c) Not relevant** |
| Cliques (Bron-Kerbosch) | BronKerboschAlgorithm | **(c) Not relevant** |
| Christofides TSP approximation | Christofides | **(c) Not relevant** — we use LS, not approximation algorithms |
| Perfect matching | PerfectMatching | **(c) Not relevant** |
| One-tree lower bound | OneTreeLowerBound | **(b) Relevant** — TSP lower bound for gap reporting |

---

## 5. Linear Solver Wrapper (MPSolver)

| Feature | Assessment |
|---------|------------|
| Glop (Google's LP solver) | **(c) Not relevant** — we are LP-free |
| SCIP integration | **(c) Not relevant** — we delegate to mip-heuristics |
| Gurobi/CPLEX/CBC wrappers | **(c) Not relevant** |
| MIP solving | **(c) Not relevant** — mip-heuristics handles this |
| PDLP (first-order LP solver) | **(c) Not relevant** |

**Assessment:** Our explicit design choice is LP-free solving for the structured
problems. Lot sizing delegates to mip-heuristics. We do not need an LP wrapper.

---

## 6. Knapsack Solver

| Feature | Assessment |
|---------|------------|
| Multi-dimensional branch-and-bound | **(c) Not relevant** — we handle packing via local search |
| Single knapsack optimal solving | **(c) Not relevant** |

---

## 7. Summary: Features to Add

### 7.1 HIGH PRIORITY (should add)

These are features that OR-Tools exposes and that real users need. They
represent gaps in our current plan.

**Routing Constraints:**

1. **Driver/vehicle breaks** — Break scheduling with min break duration and max
   time between breaks (interbreak limits). OR-Tools has BreakPropagator with
   interval-based break scheduling and cumulative constraints at depots. This is
   essential for real-world compliance (EU driving regulations, HOS rules).
   *Implementation: BreakResource with break intervals per vehicle, propagated
   along route like DurationResource.*

2. **LIFO/FIFO pickup-delivery ordering** — OR-Tools supports LIFO (stack),
   FIFO (queue), and no-order policies for pickup-delivery. We have precedence
   (pickup before delivery) but not stack/queue constraints. Important for
   physical loading (last loaded = first unloaded).
   *Implementation: extend PrecedenceResource with ordering policy.*

3. **Visit type incompatibilities** — Hard and temporal incompatibilities between
   visit types (e.g., hazmat and food cannot share a vehicle). OR-Tools has
   `AddHardTypeIncompatibility()` and `AddTemporalTypeIncompatibility()`.
   *Implementation: TypeIncompatibilityResource or move filter.*

4. **Visit type requirements** — "If carrying type A, must also carry type B"
   (e.g., hazmat requires safety equipment). OR-Tools has three flavors:
   same-vehicle, when-adding, when-removing.
   *Implementation: TypeRequirementResource.*

5. **Resource groups (depot resources)** — Shared resources at locations (loading
   docks, charging stations). OR-Tools models this with ResourceGroup +
   cumulative constraint. Essential for depot scheduling within routing.
   *Implementation: DepotResourceConstraint with cumulative capacity.*

6. **Guided Local Search (GLS)** — OR-Tools' recommended metaheuristic for VRP.
   Penalizes frequently-used arcs to escape local optima. We have strategic
   oscillation (penalty weight adjustment) but not GLS specifically. GLS is
   complementary and well-proven.
   *Implementation: GLS acceptor/penalizer in search control layer.*

7. **Multi-armed bandit operator selection** — OR-Tools uses MAB to adaptively
   select between operators based on historical performance. We have weighted
   operator selection (static weights). MAB is the adaptive version.
   *Implementation: extend OperatorSelector with MAB (UCB1 or similar).*

8. **Dimension-dependent dimensions** — One resource depending on another (fuel
   consumption depends on current load). OR-Tools:
   `AddDimensionDependentDimensionWithVehicleCapacity()`. We currently require
   embedding coupled resources in a single resource type.
   *Implementation: allow resource composition/dependency in the framework.*

9. **Span cost / global span cost** — Minimize the span (max cumul - min cumul)
   of a dimension, either per-route or globally (minimax across all routes).
   Useful for workload balancing across vehicles.
   *Implementation: span cost component in CostEvaluator.*

10. **Lower bound computation** — OR-Tools provides `ComputeLowerBound()` via
    linear assignment. Essential for gap reporting ("solution is X% from optimal").
    *Implementation: linear assignment or 1-tree lower bound at solve time.*

11. **Solution finalization** — After main optimization, OR-Tools can minimize/
    maximize secondary variables (e.g., compact departure times, minimize
    waiting). We currently optimize only the primary objective.
    *Implementation: post-optimization pass in solver.*

**Scheduling:**

12. **Reservoir constraint** — Tracks a level that changes over time with
    refill/empty events, maintaining bounds. Useful for tank scheduling,
    inventory in scheduling context.
    *Implementation: ReservoirResource in scheduling engine.*

13. **NoOverlap2D** — Prevents rectangles from overlapping. Relevant for 2D
    strip packing, warehouse layout, or physical resource allocation.
    *Implementation: 2D packing constraint if we expand packing scope.*

14. **Automaton constraint** — Finite state machine accepting/rejecting
    sequences. Powerful for pattern constraints in nurse rostering (e.g.,
    forbidden shift patterns).
    *Implementation: AutomatonConstraint in assignment engine.*

**Search:**

15. **Portfolio solving** — Running multiple search strategies in parallel,
    taking the best result. OR-Tools CP-SAT does this with `num_workers` and
    `subsolvers`. We could run ILS + HGS + SA in parallel.
    *Implementation: ParallelPortfolio in search layer.*

### 7.2 MEDIUM PRIORITY (nice to have)

16. **Amortized cost factors** — Linear/quadratic costs based on route density
    (penalize routes with few stops). Edge case but useful for fleet economics.

17. **Path energy cost** — Energy as integral of force over distance, with
    force thresholds. Relevant for electric vehicle routing with load-dependent
    energy consumption.

18. **Cumul-dependent transit callbacks** — Transit cost depends on current
    cumulative value. Models time-dependent travel where speed depends on
    departure time.

19. **Soft same-vehicle constraint** — Penalty for splitting a node group across
    vehicles (vs. hard requirement). We have hard site-dependent; soft version
    adds flexibility.

20. **Penalty cost behavior (PENALIZE_ONCE vs PER_INACTIVE)** — For groups/
    disjunctions, whether the penalty is charged once for any unserved node in
    the group or per unserved node.

21. **Absence-based acceptance** — Accept solution if total node absences
    decrease, even if cost increases. Useful for overconstrained scenarios
    where serving more nodes matters.

22. **RelocateExpensiveChain operator** — Targets the most expensive subchain
    for relocation. A smart variant of related removal.

23. **Subtrip operators** — RelocateSubtrip and ExchangeSubtrip for moving
    pickup-delivery subtrips as units. Important for PDPTW performance.

24. **Visit-type-based LNS** — LNS that removes/reinserts by visit type rather
    than geography. Useful for problems with type-based constraints.

### 7.3 LOW PRIORITY / NOT RELEVANT

- **LP-based global constraint checking** (GlobalLPCumulFilter): We are LP-free.
- **CP-SAT as sub-solver for routing**: Different approach.
- **Christofides/MST construction**: Our construction heuristics suffice.
- **Hamiltonian/Eulerian paths**: Not relevant for our problem types.
- **Clique finding**: Not relevant.
- **Pure max flow**: Not a problem we solve.
- **Knapsack solver**: We handle packing via local search.
- **Full CP-SAT solving engine**: We are a heuristic library.

---

## 8. Problem Types Coverage Comparison

### Problems OR-Tools solves that we DON'T cover:

| Problem | OR-Tools Approach | Assessment |
|---------|-------------------|------------|
| Pure TSP (exact/near-exact) | Routing + Christofides/LK | **(a) Covered** — TSP is a special case of CVRP with 1 vehicle |
| 2D bin packing / strip packing | CP-SAT NoOverlap2D | **(b) Relevant** — not in our plan, but a natural extension |
| Knapsack (exact) | Branch-and-bound | **(c) Not relevant** — different problem class |
| Graph coloring | CP-SAT AllDifferent | **(c) Not relevant** |
| N-queens | CP-SAT | **(c) Not relevant** |
| Cryptarithmetic | CP-SAT | **(c) Not relevant** |
| Generic constraint satisfaction | CP-SAT | **(c) Not relevant** — we are specialized |
| Vehicle routing + depot scheduling | Routing + cumulative at depot | **(b) Relevant** — resource groups |

### Problems we solve that OR-Tools DOESN'T:

| Problem | Notes |
|---------|-------|
| CARP (arc routing) | OR-Tools has no arc routing support |
| EVRP (electric vehicles) | No battery/recharge modeling |
| IRP (inventory routing) | No multi-period inventory |
| LRP (location-routing) | No depot selection |
| PVRP (periodic VRP) | No multi-period visits |
| cluVRP (clustered VRP) | No cluster constraints |
| VRPTF (transshipment) | No multi-echelon |
| JSP/PFSP/FJSP/OSSP (heuristic) | OR-Tools uses CP-SAT (exact), not specialized heuristics |
| RCPSP/MRCPSP (heuristic) | OR-Tools uses CP-SAT |
| Car sequencing | OR-Tools has no specific support |
| Nurse rostering (heuristic) | OR-Tools uses CP-SAT, not specialized moves |
| School timetabling | Not in OR-Tools |
| Bed allocation | Not in OR-Tools |
| Lot sizing (CLSP/MLCLSP) | Not in OR-Tools |
| VBP, BPPC (heuristic) | OR-Tools uses MIP for bin packing |
| SDVRP (split delivery) | Limited support |
| TDVRP (time-dependent) | No time-dependent travel times |

This is our core competitive advantage: specialized heuristic engines for
problems where OR-Tools either uses generic exact solvers (CP-SAT/MIP) that
don't scale, or has no support at all.

---

## 9. Architectural Differences

| Aspect | OR-Tools Routing | primal-rsp |
|--------|-----------------|------------|
| **Core approach** | Constraint programming + local search | Pure heuristic (ILS/HGS + local search) |
| **Constraint model** | Dimensions (cumulative vars + slack) | Resources (init/merge/excess) |
| **Extensibility** | Callbacks + dimension API | New resource types in C++ |
| **Move evaluation** | CP propagation + filters | O(1) incremental via prefix arrays |
| **Infeasibility** | Hard constraints (infeasible = fail) | Penalized violations (strategic oscillation) |
| **Multi-objective** | Not supported | Not supported (weighted sum) |
| **Scaling** | Good to ~5000 nodes | Target: 10,000+ nodes (AILS-II approach) |
| **Scheduling** | CP-SAT (exact, interval vars) | Specialized heuristics (disjunctive graph) |
| **Assignment** | CP-SAT (exact, boolean vars) | Specialized moves (swap/reassign + tabu) |
| **Language** | C++ with Python/Java/C# wrappers | C++ (wrappers later) |
| **Search control** | Limited (one metaheuristic at a time) | Composable (multiple acceptors, VND, oscillation) |
| **Construction** | 14+ strategies | Nearest-neighbour, savings, cheapest insertion, FFD |
| **Population** | No genetic algorithm | HGS with population diversity |

---

## 10. Action Items (Ranked)

### Must Add (before release or early phases)

1. **Driver breaks** (BreakResource) — compliance requirement for real-world use
2. **GLS metaheuristic** — OR-Tools' top recommendation, proven effective
3. **MAB operator selection** — adaptive version of our weighted selection
4. **Lower bound computation** — gap reporting is essential for user confidence
5. **Visit type incompatibilities** — common real-world constraint
6. **LIFO/FIFO PD ordering** — physical loading constraint
7. **Resource groups (depot resources)** — depot scheduling within routing

### Should Add (medium-term)

8. **Span cost / global span** — workload balancing objective
9. **Dimension-dependent dimensions** — coupled resource tracking
10. **Solution finalization** — secondary variable optimization
11. **Automaton constraint** — pattern constraints in nurse rostering
12. **Subtrip operators** — PDPTW performance
13. **Reservoir constraint** — tank/inventory scheduling
14. **Portfolio solving** — parallel multi-strategy search
15. **Amortized cost factors** — fleet economics

### Consider (long-term)

16. **NoOverlap2D** — 2D packing extension
17. **Path energy cost** — EVRP with load-dependent energy
18. **Cumul-dependent transit** — advanced time-dependent routing
19. **Visit-type LNS** — type-aware neighborhood search
20. **Absence-based acceptance** — overconstrained-specific acceptance

---

## 11. Key Takeaways

1. **OR-Tools' Dimension concept maps directly to our Resource concept.** The
   approaches are equivalent in modeling power. Our merge-based O(1) evaluation
   is more efficient than their CP propagation for local search.

2. **OR-Tools has more routing constraint types than we planned.** Driver breaks,
   visit type incompatibilities/requirements, resource groups, and LIFO/FIFO PD
   ordering are the main gaps. All are implementable as new resources or filters
   without architectural changes.

3. **OR-Tools' search is weaker than ours.** They have one metaheuristic at a
   time, no population-based search (HGS), no composable acceptors, no strategic
   oscillation. Their ILS implementation is recent and less sophisticated than
   AILS-II. GLS is their only notable search feature we lack.

4. **OR-Tools uses CP-SAT for scheduling and assignment.** This is exact solving,
   which doesn't scale for large instances. Our specialized heuristic engines
   for JSP/RCPSP/nurse rostering will outperform on large instances, which is
   our target market.

5. **OR-Tools has no coverage of many problem types we handle.** CARP, EVRP, IRP,
   LRP, PVRP, cluVRP, VRPTF, car sequencing, school timetabling, bed allocation,
   lot sizing — these are all unique to our library.

6. **The graph algorithm library is not competitive with us.** Those are utility
   algorithms (Dijkstra, MST, etc.), not solvers. We may want some for
   subroutines (lower bounds, construction) but they are not competitive threats.

7. **OR-Tools' bin packing uses MIP, not local search.** Our assignment-engine-based
   bin packing with move/swap operators will be more flexible and scale better
   for large instances.

8. **The main thing to learn from OR-Tools is completeness of real-world routing
   constraints.** Their decade of production use has surfaced every constraint
   type users actually need. Driver breaks, type incompatibilities, depot
   resources, and LIFO/FIFO ordering come from real user demand, not academic
   problem definitions.
