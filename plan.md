# primal-rsp — Primal Heuristics for Routing, Scheduling & Production Planning

Declarative modeling + LP-free solving for structured combinatorial optimization.

Think MIP modeling (CPLEX/Gurobi) but for routing, scheduling, and planning:
the user declares **what** the problem is, the solver decides **how** to solve it.

Sibling to `mip-heuristics` (LP-free MIP solvers: FJ, Local-MIP). That repo
handles generic MIP (`Ax ≤ b`). This repo handles problems with **exploitable
structure** — routes, sequences, schedules, assignments, packing — where
problem-specific local search dominates generic approaches by orders of magnitude.

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
uses LoadResource, standard operators, ILS/HGS.

### 1.2 VRPTW with heterogeneous fleet — just add parameters

```cpp
primal::Model m;
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

auto result = m.solve(primal::TimeLimit(60));
```

**Still zero implementation.** The model sees time windows → adds DurationResource.
Sees heterogeneous fleet → adds vehicle-type-aware evaluation. Multiple load
dimensions → LoadResource tracks all dimensions. The user just declares
attributes, never touches the engine.

### 1.3 Multi-trip with overtime

```cpp
primal::Model m;
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

auto result = m.solve(primal::TimeLimit(60));
```

### 1.4 Paired pickup-delivery with time windows

```cpp
primal::Model m;
auto depot = m.add_depot(0, 0, {.tw = {0, 1000}});
m.add_vehicle_type(5, {.capacity = 20});

// Request: pick up at location A, deliver to location B (same route, in order)
auto p1 = m.add_pickup(10, 20, {.quantity = 5, .tw = {100, 200}, .service = 10});
auto d1 = m.add_delivery(30, 40, {.tw = {150, 300}, .service = 10});
m.add_request(p1, d1);  // p1 and d1 on same route, p1 before d1

auto p2 = m.add_pickup(50, 60, {.quantity = 8, .tw = {200, 400}});
auto d2 = m.add_delivery(70, 80, {.tw = {250, 500}});
m.add_request(p2, d2);

auto result = m.solve(primal::TimeLimit(60));
```

### 1.5 Optional clients and client groups

```cpp
primal::Model m;
// ...

// Optional clients with prizes (Team Orienteering)
m.add_client(10, 20, {.demand = 5, .required = false, .prize = 100});

// Client groups: exactly one from each group must be served
auto g1 = m.add_client_group();
m.add_client(10, 20, {.demand = 5, .group = g1});  // alternative locations
m.add_client(12, 22, {.demand = 5, .group = g1});  // for same customer

// Release times: client not available until a given time
m.add_client(30, 40, {.demand = 10, .release_time = 120});

auto result = m.solve(primal::TimeLimit(60));
```

### 1.6 Multi-commodity flow — fractional paths

For problems where flow can be split across paths (multi-commodity flow, network
design), the model supports fractional path variables:

```cpp
#include <primal/network.h>

primal::NetworkModel m;

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

auto result = m.solve(primal::TimeLimit(60));

// Fractional solution: flow per arc per commodity
for (auto& [commodity, paths] : result.flows()) {
    for (auto& [path, flow] : paths)
        std::cout << "Commodity " << commodity << ": flow " << flow << "\n";
}
```

This connects to flowty-core's column generation approach. For problems with
resource constraints on paths (RCMCF), the solver uses pricing with resource-
constrained shortest paths. For simpler MCF, direct LP or network simplex.

### 1.7 Job shop scheduling — same pattern, different domain

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

### 1.8 Nurse rostering — assignment with tabu search

```cpp
#include <primal/assignment.h>

primal::AssignmentModel m;

// Shift types
auto early = m.add_shift_type("Early", {.start = 7, .end = 15});
auto late  = m.add_shift_type("Late",  {.start = 15, .end = 23});
auto night = m.add_shift_type("Night", {.start = 23, .end = 7});

// Employees with skills and contracts
auto alice = m.add_employee("Alice", {.skills = {"ICU", "ER"}, .max_hours = 40});
auto bob   = m.add_employee("Bob",   {.skills = {"ER"},        .max_hours = 36});
auto carol = m.add_employee("Carol", {.skills = {"ICU"},       .max_hours = 40});

// Planning horizon
m.set_horizon(28);  // 4 weeks

// Demands: minimum staff per shift per day
m.add_demand(early, {.min_employees = 2, .required_skill = "ICU"});
m.add_demand(late,  {.min_employees = 1});
m.add_demand(night, {.min_employees = 1});

// Constraints
m.add_constraint(primal::MaxConsecutiveShifts(5));
m.add_constraint(primal::MinRestBetweenShifts(11));  // hours
m.add_constraint(primal::MaxNightShiftsPerWeek(2));
m.add_constraint(primal::WeekendBalancing());

// Preferences (soft constraints)
alice.prefer_off({5, 12, 19, 26});  // Saturdays off
bob.prefer_shift(early);             // prefers early shifts

auto result = m.solve(primal::TimeLimit(60));

for (int day = 0; day < 28; ++day)
    for (auto& assignment : result.day(day))
        std::cout << assignment.employee << " → " << assignment.shift << "\n";
```

**Same pattern: declare what, solver decides how.** The model sees employees +
shifts + constraints → uses assignment-specific operators (swap shift, reassign
employee, swap employees) with tabu search + late acceptance. No routes or
sequences — but the moves are structure-aware, not generic MIP variable flips.

### 1.9 Bin packing — items into bins

```cpp
#include <primal/packing.h>

primal::PackingModel m;

// Bin type with capacity
m.add_bin_type(100, {.capacity = {150, 200}});  // 100 bins, weight + volume

// Items with sizes
m.add_item({.size = {30, 40}});
m.add_item({.size = {50, 60}});
m.add_item({.size = {20, 30}});
// ...

m.minimize_bins();
auto result = m.solve(primal::TimeLimit(30));

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

### 1.10 Lot sizing — MIP substructure, delegates to mip-heuristics

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

---

## 2. What the Model Recognizes (Out of the Box)

The modeling layer maps declared attributes to internal engine components
automatically:

### Routing

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

### Network / Flow

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| Arcs + commodities + demands | Multi-commodity flow | Network simplex / CG |
| Arc resources + bounds | Resource-constrained MCF | Column generation + RCSPP |

### Scheduling

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| Operations with `machine` + `duration` | Job shop | Disjunctive graph |
| Same machine order for all jobs | Flow shop | Permutation schedule |
| `setup_time` between ops | Sequence-dependent setup | Extended evaluation |
| `due_date` on job | Tardiness | Penalty term |
| `release_date` on job | Release dates | Feasibility check |
| Multiple machines per op | Flexible job shop | Assignment + sequencing |
| Activities + precedences + resource reqs | RCPSP | Activity list + SGS |
| Multiple machines, independent jobs | Parallel machine | Assignment + sequencing |
| No intra-job precedence | Open shop | Disjunctive graph (no job arcs) |
| Multiple modes per activity | Multi-mode RCPSP | Mode selection + SGS |
| `optional = true` on operation | Optional operations | Overconstrained scheduling |
| Sliding-window ratio constraints | Car sequencing | Permutation + window eval |
| `reservoir` resource with events | Reservoir constraint | Level tracking (refill/empty) |

### Assignment / Timetabling

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| Employees + shifts + constraints | Nurse rostering / employee scheduling | Assignment engine (tabu + VND) |
| Rooms + timeslots + lectures | School timetabling | Assignment engine (tabu + VND) |
| Talks + rooms + timeslots | Conference scheduling | Assignment engine (tabu + VND) |
| Patients + beds + stays | Bed allocation | Assignment engine (tabu + VND) |
| More entities than capacity | Overconstrained | Unassigned with medium penalty |
| `weight` on constraint | Configurable priorities | Weighted soft constraints |
| `forbidden_sequence(N, N, ...)` | Shift pattern rules | Automaton constraint (FSM) |

### Packing

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| Items + bins + capacity | Bin packing | Assignment engine (move/swap items) |
| Multi-dim `size` + `capacity` | Vector bin packing | Multi-dim capacity per bin |
| `conflict(a, b)` | Bin packing with conflicts | Conflict constraint |
| `minimize_bins()` | Minimize bins used | Bin count objective |

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
│  Move Evaluation (resources + CostEvaluator)            │
│  → Extensibility point for new constraints              │
│  → New constraint = new resource type in C++            │
├─────────────────────────────────────────────────────────┤
│  Move Topology (operators)                              │
│  → How nodes/jobs get rearranged                        │
│  → Reusable across constraint variants                  │
│  → Clean function signatures for LLM operator discovery │
└─────────────────────────────────────────────────────────┘
```

- **Operators** describe topology changes (relocate, swap, 2-opt). They don't
  know about constraints. Reusable across all VRP variants.
- **Resources** propagate constraint state along routes. Adding a new constraint =
  adding a resource type in C++. Operators unchanged.
- **CostEvaluator** sums penalty terms from all active resources plus cost
  components (fixed, distance, duration, overtime). Operators call it to price
  out moves.
- **Algorithms** (ILS, HGS) compose operators + local search + penalty management.
  Each is its own class.

### 3.2 Resources: the extensibility mechanism

A resource is a function on subsequences with an associative composition operator.
Each resource tracks some aspect of constraint satisfaction along a route (load,
time, pickup state, ...) and reports violations as penalties.

The resource interface is a small set of functions. Adding a new resource means
adding a C++ file that implements these functions — no changes to operators,
algorithms, or other resources.

```cpp
struct LoadResource {
    // State type (forward and reverse can differ for direction-sensitive resources)
    struct State { int delivery, pickup, load; };

    // Initial state at depot
    static State init();

    // Merge two consecutive subsequences
    static State merge(State left, State right);

    // Merge when right subsequence is reversed (needed for 2-opt)
    // Direction-independent resources: merge_reverse = merge
    static State merge_reverse(State left, State right);

    // Penalty for constraint violation
    static int excess(State s, int capacity);
};

struct DurationResource {
    struct State { int duration, time_warp, tw_early, tw_late, release_time; };

    static State init();
    static State merge(State left, State right, int edge_duration);
    static State merge_reverse(State left, State right, int edge_duration);
    static int excess(State s);     // total_time_warp
};
```

LoadResource supports multiple dimensions — internally an array of State per
dimension, each independently checking `excess(s, capacity[d])`.

The Route maintains prefix/suffix arrays for each active resource. Move evaluation
merges subsequence states in O(1) to compute the delta cost without re-scanning
the route.

**Resource independence.** Each resource is self-contained. Load doesn't know
about time. Time doesn't know about load. The CostEvaluator sums penalties
independently: `distance + α·excess_load + β·time_warp + γ·excess_distance + ...`

If two resources genuinely depend on each other (e.g., cumulative cost =
weight × time), embed both in a single resource. This is an explicit design
choice, not a framework coupling.

**Forward/reverse state asymmetry.** Some resources need different state types
for forward vs. reverse propagation. This matters for 2-opt (which reverses a
subsequence) and correct O(1) move evaluation.

Example: Simultaneous Pickup-and-Delivery. At every point along the route,
`collected_pickup + remaining_delivery ≤ capacity`. Forward tracking needs
`(P, D̂)` — collected pickup and max remaining delivery. Reverse tracking needs
`(P̂, D)` — max remaining pickup and delivered demand.

Most built-in resources are symmetric (merge = merge_reverse). The interface
supports asymmetry when needed without burdening simple cases.

**Adding a new resource.** To support a new constraint type, a developer:

1. Defines a `State` struct with the fields needed for propagation
2. Implements `init()`, `merge()`, `merge_reverse()`, `excess()`
3. Registers the resource so the Model activates it when relevant attributes
   are declared

No changes to operators, local search, algorithms, or other resources. The
CostEvaluator picks up the new penalty term automatically.

### 3.3 Cost model

The CostEvaluator computes the total cost of a solution:

```
cost = Σ_route (
    distance(route) × unit_distance_cost(vehicle)
  + duration(route) × unit_duration_cost(vehicle)
  + fixed_cost(vehicle)                           // if route is non-empty
  + overtime(route) × unit_overtime_cost(vehicle)  // soft duration excess
  - Σ_client prize(client)                         // for optional clients served
  + α · excess_load(route)                         // penalty: capacity violation
  + β · time_warp(route)                           // penalty: time window violation
  + γ · excess_distance(route)                     // penalty: max distance violation
  + ...                                            // additional resource penalties
)
```

Penalty weights (α, β, γ) are managed by PenaltyManager with adaptive adjustment
targeting a configurable feasibility ratio.

**Piecewise linear cost functions.** For tiered pricing, volume discounts, and
multi-tier overtime: define cost as breakpoints `[(x0,y0), (x1,y1), ...]`.
Evaluated via lookup table in the CostEvaluator. E.g., overtime at 1.5x for
first 2 hours, 2x beyond.

### 3.4 Operators

**Binary operators** (operate on pairs of route segments):

| Operator | Description |
|---|---|
| Exchange(1,0) | Relocate one client |
| Exchange(2,0) | Relocate two consecutive clients |
| Exchange(3,0) | Relocate three consecutive clients |
| Exchange(1,1) | Swap single clients |
| Exchange(2,1) | Swap two for one |
| Exchange(2,2) | Swap two for two |
| Exchange(3,1) | Swap three for one |
| Exchange(3,2) | Swap three for two |
| Exchange(3,3) | Swap three for three |
| SwapTails | 2-opt: swap tail segments between routes |
| SWAP* | Vidal's SWAP* with star-cost precomputation |

**Unary operators** (operate on single routes or optional clients):

| Operator | Description |
|---|---|
| InsertOptional | Insert optional client into best position |
| RemoveOptional | Remove optional client from route |
| ReplaceGroup | Swap which client serves a mutually exclusive group |
| RelocateWithDepot | Multi-trip-aware relocation past reload depot |
| RelocatePair | Move pickup-delivery pair together |
| SwapPairs | Swap pickup-delivery pairs between routes |
| RelocateSubtrip | Move contiguous pickup-delivery subtrip |
| ExchangeSubtrip | Exchange subtrips between routes |

**Perturbation operators** (for ILS ruin-and-recreate):

| Operator | Description |
|---|---|
| RandomRemoval | Remove random clients |
| WorstRemoval | Remove highest-cost clients |
| RelatedRemoval | Remove geographically close clients |
| GreedyRepair | Reinsert at cheapest position |

### 3.5 Search control features

**Composable acceptors.** Acceptance criteria can be combined: e.g., late acceptance
+ tabu simultaneously. Each acceptor votes accept/reject; a composite combines
them (any-accept, all-accept, weighted). This avoids single-strategy stagnation.

**Ruin-and-recreate as move type.** Large-neighbourhood moves (remove k clients,
reinsert) are regular moves evaluated by the same acceptance criterion as local
search moves. Not a separate phase — just another operator in the neighbourhood.

**Strategic oscillation.** PenaltyManager deliberately oscillates between feasible
and infeasible regions. When solutions are mostly feasible, reduce penalties to
explore infeasible shortcuts. When mostly infeasible, increase penalties to drive
back to feasibility. Target ratio ~25% infeasible.

**Warm start / pinning.** Users can provide an initial solution and optionally pin
(lock) specific assignments. Pinned entities are excluded from moves. Useful for
re-optimization (new orders added to existing routes) and what-if analysis.

**Diminished returns termination.** Beyond simple time/iteration limits, detect
when improvement rate has stalled. Track best-cost improvement over a rolling
window; terminate when improvement drops below threshold.

**Benchmarker.** Parameter tuning infrastructure: define parameter grids, run
multiple seeds per configuration, collect statistics (mean, std, best, worst),
produce comparison tables. Not part of the solver itself, but a first-class tool
for development and user experimentation.

**Multi-level scoring.** Support hard/soft constraint separation with priority
levels. Hard constraints (capacity, time windows) must be satisfied; soft
constraints (overtime, optional clients) are optimized. Internally this maps to
the existing penalty framework: hard constraints get very high penalty weights,
soft constraints get finite weights. The user declares which constraints are
hard vs. soft; the PenaltyManager handles the rest.

**Guided Local Search (GLS).** Penalizes frequently-used solution features (arcs
in routing, assignments in scheduling) to escape local optima. Maintains penalty
memory across iterations. OR-Tools' #1 recommended VRP metaheuristic.
Complementary to strategic oscillation — GLS diversifies by feature frequency,
oscillation diversifies by feasibility boundary.

**Weighted operator selection with multi-armed bandit.** Probability weights on
move types control how often each operator is tried. Static weights configurable
per problem type. Adaptive mode uses multi-armed bandit (UCB or Thompson
Sampling) to learn which operators are productive during the search, adjusting
weights online. Avoids uniform random wasting time on low-yield operators.

**Move filters.** User-provided predicates that skip certain moves before
evaluation. E.g., "vehicle type X cannot serve customer Y" or "employee A never
works night shifts". Applied at the operator level before cost evaluation,
avoiding wasted computation. Different from hard constraints — filters are
absolute exclusions, not penalized violations.

**Accepted count limit / pick-early.** Two knobs for large instances:
- `accepted_count_limit`: evaluate at most N candidate moves per step, then pick
  the best seen. Controls time per step.
- `pick_early`: stop evaluating when first improving move is found. Faster
  iterations, more iterations per second, slightly greedier.
Critical for scaling to 5000+ node instances where evaluating all neighbours
is too slow even with granular neighbourhood.

**Score corruption detection.** Debug mode (`PRIMAL_ASSERT_SCORES`) that
recalculates the full cost from scratch after each move and asserts it matches
the incremental O(1) evaluation. Catches resource implementation bugs during
development. Also supports undo-move verification: apply move, undo, assert
original score restored. Disabled in release builds (zero overhead).

**Score explanation / analysis.** After solving, the result object provides a
structured breakdown of constraint violations: which routes/assignments violate
which constraints, by how much, and which entities are responsible. "Route 3
violates time window at customer 7 by 15 minutes." Essential for user trust
and debugging model declarations.

**Non-disruptive replanning.** When warm-starting from a published solution,
optionally penalize deviations: `penalty_weight * changes_from_published`. The
solver only makes changes that improve enough to justify the disruption. Natural
in the penalty framework — just another soft cost term.

**Overconstrained planning.** When not all entities can be assigned (more shifts
than available staff, more clients than vehicle capacity), allow unassigned
entities with a medium-priority penalty (between hard and soft). For routing,
this maps to optional clients. For assignment, explicit support for unassigned
shifts that the solver tries to minimize.

**User-configurable constraint weights.** For assignment/timetabling, users set
relative importance of soft constraints at model definition time:
`m.add_constraint(MaxConsecutiveShifts(5), {.weight = 10})`. The solver respects
these weights in the penalty function. Supports runtime weight adjustment for
what-if analysis.

**Variable Neighborhood Descent (VND).** Ordered neighborhood exploration: try
move type 1 exhaustively, then type 2, then type 3. On finding an improving
move, reset to type 1. Useful for assignment/timetabling where different move
types have different strengths. E.g., reassign → swap → pillar swap.

**Partitioned search.** For very large instances (5000+ nodes), partition the
problem (e.g., geographic clusters for VRP, department groups for scheduling),
solve each partition independently, then optimize cross-partition moves. Achieves
faster initial solutions. Optionally followed by non-partitioned local search
to fix partition boundary effects.

**Daemon mode / continuous solving.** Long-running solver that accepts problem
changes via a concurrent queue without restarting. When a new order arrives or
an employee calls in sick, the solver integrates the change and continues
optimizing. For real-time dispatch (ride-sharing, same-day delivery) and
rolling-horizon scheduling.

**Portfolio solving.** Run ILS + HGS + SA in parallel on the same instance,
take the best result. Each thread uses a different metaheuristic with different
parameter configurations. Shared solution pool allows cross-pollination
(crossover restarts from pool). Strictly better than multi-start within one
algorithm when the search strategies are complementary.

**Solution finalization.** Post-optimization pass that compacts secondary
variables without changing the route/assignment structure. For routing: minimize
total waiting time by adjusting departure times. For scheduling: compact idle
gaps. Runs after the main optimization phase, polishing the solution.

### 3.6 Construction heuristics

**Routing:** Nearest-neighbour (default), savings algorithm (Clarke-Wright).
Both seed routes, then local search improves.

**Assignment:** First Fit Decreasing — sort entities by difficulty (most-
constrained employee first: fewest available shifts, most skill requirements),
assign to best available value. Cheapest insertion — evaluate all entity-value
pairs, pick the one with lowest cost increase.

**Scheduling:** NEH (flow shop), priority-rule dispatch (job shop), serial/
parallel SGS (RCPSP).

### 3.7 Assignment engine operators

**Basic moves:**

| Operator | Description |
|---|---|
| ChangeShift | Reassign one employee to a different shift |
| SwapShifts | Exchange shifts between two employees |
| SwapEmployees | Exchange all assignments between two employees for a period |

**Pillar moves** (move groups with identical assignments):

| Operator | Description |
|---|---|
| PillarChange | Move all employees assigned to shift A to shift B |
| PillarSwap | Exchange all employees between two shifts |

Pillar moves are powerful for timetabling where groups of entities share values
(e.g., all lectures in room A move to room B).

### 3.8 LLM-friendly operator interfaces

For LLM-driven heuristic discovery (VRPAgent, EoH, FunSearch):

```cpp
// Clean function signatures LLMs can target
using DestroyFn = std::function<std::vector<int>(Solution&, int count, RNG&)>;
using RepairFn  = std::function<void(Solution&, const std::vector<int>&,
                                      const ProblemData&)>;
using PerturbFn = std::function<Solution(const Solution&, const ProblemData&, RNG&)>;
```

---

## 4. Problem Catalog

Complete list of target problems with benchmark instance availability.

### 4.1 Routing problems

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

### 4.2 Network / flow problems

| # | Problem | Abbrev | Approach | Phase | Benchmarks | Instances | Source |
|---|---|---|---|---|---|---|---|
| N1 | Multi-Commodity Flow | MCF | Network simplex / LP | 6+ | SNDlib, Canad | varies | SNDlib |
| N2 | Resource-Constrained MCF | RCMCF | Column generation + RCSPP | 6+ | — | varies | — |

These support fractional paths (flow split across routes). Unlike pure VRP where
each route is an integer path, MCF allows fractional flow on arcs. This is
meaningful for network design, telecom routing, and logistics network planning
where demand can be split. Column generation with pricing (as in flowty-core)
is the natural approach.

### 4.3 Scheduling problems

| # | Problem | Abbrev | Approach | Phase | Benchmarks | Instances | Source |
|---|---|---|---|---|---|---|---|
| S1 | Job Shop Scheduling | JSP | Disjunctive graph | 5 | Taillard, OR-Library, DMU | ~250+ | JSPLIB |
| S2 | Permutation Flow Shop | PFSP | Permutation schedule | 5 | Taillard, VFR | 120+480 | SOA/Taillard |
| S3 | Flexible Job Shop | FJSP | Assignment + sequencing | 5 | Brandimarte, Hurink, DMU | ~467 | FJSPLIB |
| S4 | RCPSP | RCPSP | Activity list + SGS | 5 | PSPLIB J30/J60/J90/J120 | 2,040 | PSPLIB |
| S5 | JSP with Setup Times | JSP-SDST | Extended evaluation | 5+ | TU/e platform | varies | GitHub |
| S6 | Parallel Machine | P\|\|Cmax | Assignment + sequencing | 5+ | Texas DataVerse | varies | DataVerse |
| S7 | Unrelated Parallel Machine | R\|\|Cmax | Assignment + sequencing | 5+ | Vallada-Ruiz | 640+ | SOA |
| S8 | Hybrid Flow Shop | HFS | Stage assignment + seq | 5+ | Ruiz-Rodriguez | 480+ | SOA |
| S9 | Open Shop | OSSP | Disjunctive graph (no job arcs) | 5 | Taillard, Guéret-Prins | ~80+ | JSPLIB |
| S10 | Multi-Mode RCPSP | MRCPSP | Mode selection + SGS | 5+ | MMLIB, PSPLIB | ~900+ | MMLIB/PSPLIB |
| S11 | Preemptive RCPSP | PRCPSP | Task splitting + SGS | 8+ | PSPLIB preemptive | varies | PSPLIB |
| S12 | Car Sequencing | — | Permutation + window eval | 8+ | CSPLib prob001 | ~80 | CSPLib |

### 4.4 Assignment / timetabling problems

| # | Problem | Abbrev | Approach | Phase | Benchmarks | Instances | Source |
|---|---|---|---|---|---|---|---|
| A1 | Nurse Rostering | NRP | Assignment engine (tabu + LA) | 6 | INRC-I, INRC-II | 138+ | INRC |
| A2 | Employee Scheduling | ESP | Assignment engine (tabu + LA) | 6 | Curtois nurse datasets | 24+ | Scheduling Benchmarks |
| A3 | School Timetabling | — | Assignment engine (tabu + LA) | 6+ | ITC-2007, ITC-2019 | varies | ITC |
| A4 | Conference Scheduling | — | Assignment engine (tabu + LA) | 6+ | — | — | — |
| A5 | Bed Allocation | BAS | Assignment engine (tabu + LA) | 6+ | Ceschia-Schaerf | 15+ | Papers |

### 4.5 Packing problems

| # | Problem | Abbrev | Approach | Phase | Benchmarks | Instances | Source |
|---|---|---|---|---|---|---|---|
| K1 | Bin Packing | BPP | Assignment engine (move/swap) | 6+ | Scholl, Falkenauer | ~1,370 | BPPLIB |
| K2 | Vector Bin Packing | VBP | Multi-dim capacity per bin | 6+ | Caprara-Toth | varies | Papers |
| K3 | Bin Packing w/ Conflicts | BPPC | + conflict constraint | 8+ | Muritiba et al. | varies | Papers |

### 4.6 Production planning problems

| # | Problem | Abbrev | Approach | Phase | Benchmarks | Instances | Source |
|---|---|---|---|---|---|---|---|
| P1 | Capacitated Lot Sizing | CLSP | Fix-and-Optimize (MIP) | 6 | Trigeiro | 540 | Suerie |
| P2 | Multi-Level CLSP | MLCLSP | Fix-and-Optimize (MIP) | 6 | Tempelmeier-Derstroff | ~1920 | Uni Cologne† |

† Original URLs may be defunct. Instances typically regenerated from published parameters.

### 4.7 Instance format support

| Format | Covers | Parser needed |
|---|---|---|
| VRPLIB (TSPLIB extension) | CVRP, VRPTW, HFVRP, VRPB, MDVRP, MTVRP, TOP | Yes (Phase 1) |
| Solomon | VRPTW, PDPTW (Li-Lim) | Yes (Phase 4) |
| Cordeau | MDVRP, PVRP | Yes (Phase 4) |
| Taillard (scheduling) | JSP, PFSP | Yes (Phase 5) |
| OR-Library (scheduling) | JSP | Yes (Phase 5) |
| `.fjs` (FJSPLIB) | FJSP | Yes (Phase 5) |
| `.sm` / `.mm` (PSPLIB) | RCPSP | Yes (Phase 5) |
| INRC-II | NRP | Yes (Phase 6) |
| ITC (timetabling) | School timetabling | Yes (Phase 6) |
| Curtois | Employee scheduling | Yes (Phase 6) |
| BPPLIB | BPP, VBP | Yes (Phase 6+) |
| MMLIB (multi-mode) | MRCPSP | Yes (Phase 5+) |
| Guéret-Prins | OSSP | Yes (Phase 5) |
| CSPLib prob001 | Car sequencing | Later |
| CARP format | CARP | Later |
| DIMACS VRP | SDVRP, EVRP, IRP, CARP | Later |

### 4.8 What the modeling approach handles well vs. not

**Handles well (resource pattern):** Problems where the solution is a set of
sequences (routes, job orderings) and constraints can be evaluated by propagating
state along each sequence. R1–R15, R17, R20, R24–R26, S1–S12.

**Handles with structural changes:** Problems that change the solution
representation — split delivery (R18: client in multiple routes), paired
pickup-delivery (R15: precedence within route), multi-trip (R12: depot visits
mid-route), period VRP (R21: multi-day). Each needs operators adapted to the
structure, not just new resources.

**Handles via transformation:** CARP (R16) → node routing. Time-dependent VRP
(R19) → time-indexed distance functions.

**Handles via decomposition:** Location-routing (R23: depot selection + routing),
inventory routing (R22: multi-period routing + inventory). Decompose into
subproblems; routing subproblem uses the standard engine.

**Network / fractional:** MCF and RCMCF (N1, N2) support fractional paths.
Uses LP / column generation rather than local search. Meaningful for network
design where flow can be split.

**Assignment / timetabling (structure-aware, not MIP).** Problems like nurse
rostering, employee scheduling, school timetabling, conference scheduling, and
bed allocation (A1–A5) lack sequential structure but DO have exploitable
assignment structure. Timefold proves that tabu search + late acceptance with
problem-specific moves (swap shift, reassign employee, swap employees) works
well commercially. We use the same approach: AssignmentModel compiles to an
assignment engine with structure-aware operators, reusing the search control
layer (composable acceptors, tabu, penalty management). Not delegated to MIP —
native moves over the assignment representation outperform generic variable flips.

**Packing (assignment engine).** Bin packing, vector bin packing, bin packing
with conflicts (K1–K3) use the assignment engine: items assigned to bins,
partition constraint implicit, move/swap operators. Capacity tracked per bin.
Same engine as nurse rostering, different operators.

**Delegates to mip-heuristics:** Lot sizing (P1, P2) — MIP substructure where
Fix-and-Optimize with FJ/Local-MIP as inner solver is the natural approach.

---

## 5. Source Layout

```
src/
  model/              ← User-facing modeling API
    model.h               Routing model (add_depot, add_client, add_vehicle_type)
    network_model.h       Network/flow model (add_node, add_arc, add_commodity)
    schedule_model.h      Scheduling model (add_job, add_operation)
    assignment_model.h    Assignment model (add_employee, add_shift_type)
    packing_model.h       Packing model (add_bin_type, add_item)
    lotsizing_model.h     Lot sizing model
    instance_reader.h     CVRPLIB, Solomon, Cordeau, Taillard parsers

  routing/            ← CVRP/VRPTW engine (internal)
    problem_data.h        Compiled instance data (distances, profiles, attributes)
    solution.h            Route-based solution
    route.h               Route with resource-based evaluation
    resources/
      load_resource.h         Capacity (N dimensions)
      duration_resource.h     Time windows, release times, overtime
      distance_resource.h     Max route distance
      pickup_delivery_resource.h
      precedence_resource.h   Paired PD: pickup before delivery on same route
      cumulative_cost_resource.h
      cluster_resource.h      Clustered VRP: finish cluster before leaving
      break_resource.h        Driver breaks: min duration, max interbreak time
      type_incompatibility.h  Hazmat/type conflicts between visits
      type_requirement.h      Co-presence requirements on same vehicle
      depot_resource.h        Shared capacity at depots (loading docks)
    cost_evaluator.h      Fixed + variable + overtime + prize + penalties
    operators/
      exchange.h            Exchange(N,M) for N,M ∈ {0..3}
      swap_star.h           SWAP* with star-cost precomputation
      insert_optional.h     Insert/remove optional clients
      replace_group.h       Swap group representatives
      relocate_with_depot.h Multi-trip-aware relocation
      pair_operators.h      Relocate/swap pickup-delivery pairs
    perturbation/         Destroy + Repair operators
    local_search.h
    penalty_manager.h
    neighbours.h

  network/            ← MCF / RCMCF engine
    network_data.h
    flow_solution.h

  search/             ← Metaheuristic shells
    iterated_local_search.h
    genetic_algorithm.h
    population.h
    crossover.h
    stop_criterion.h      Runtime, iterations, no-improve limit, diminished returns
    acceptance.h          Late acceptance, SA, tabu, composable
    operator_selector.h   Weighted probability selection over move types
    move_filter.h         User predicate to skip moves before evaluation
    penalty_manager.h     Adaptive penalties, strategic oscillation
    warm_start.h          Pin entities, partial solution injection, replanning penalty
    score_analysis.h      Constraint violation breakdown per entity
    score_assert.h        Debug: full recalc vs incremental assertion
    benchmarker.h         Parameter grid, multi-seed, score-over-time tracking
    guided_local_search.h GLS: penalize frequently-used features
    portfolio.h           Run multiple metaheuristics in parallel
    solution_finalizer.h  Post-optimization: compact waiting/idle
    partitioned_search.h  Decompose large instances into independent partitions
    daemon.h              Continuous solving with problem change queue

  scheduling/         ← JSP/FSP/RCPSP/OSSP engine
    disjunctive_graph.h
    schedule_solution.h
    schedule_operators.h
    mode_selection.h      Multi-mode RCPSP: mode assignment per activity
    car_sequencing.h      Sliding-window ratio constraints on permutation

  assignment/         ← Nurse rostering / timetabling engine
    assignment_data.h     Compiled from AssignmentModel
    assignment_solution.h Employee-shift assignments
    operators/
      change_shift.h        Reassign employee to different shift
      swap_shifts.h         Exchange shifts between two employees
      swap_employees.h      Exchange all assignments for a period
      pillar_change.h       Move group with same value together
      pillar_swap.h         Exchange groups between two values
    construction.h        First Fit Decreasing, Cheapest Insertion
    constraints/
      consecutive_shifts.h
      rest_between_shifts.h
      weekend_balancing.h
      skill_matching.h
      load_balancing.h      Fairness / workload distribution
      automaton.h           FSM for forbidden shift sequences (e.g., N-N-E)

  packing/            ← Bin packing engine (reuses assignment infrastructure)
    packing_data.h        Compiled from PackingModel
    packing_solution.h    Item-to-bin assignments
    packing_operators.h   MoveItem, SwapItems between bins
    bin_capacity.h        Per-bin capacity tracking (N dimensions)

  lotsizing/          ← CLSP engine

  cli/
    main.cpp

tests/
  model/              ← User-facing tests (the API contract)
  routing/
  network/
  scheduling/
  assignment/
  packing/
  search/
  data/
```

---

## 6. Implementation Phases

### Phase 1 — CVRP end-to-end

User can: `m.add_depot(); m.add_client({.demand=...}); m.solve();`

Engine:
1. Repo skeleton (CMake, C++23, Catch2)
2. `Model` class with `add_depot`, `add_client`, `add_vehicle_type`, `solve`
3. CVRPLIB/VRPLIB parser
4. `ProblemData` (compiled from Model)
5. `LoadResource` with `init/merge/merge_reverse/excess`
6. `Route` with prefix resource arrays
7. `Solution` + `CostEvaluator`
8. Operators: Exchange(1,0), Exchange(1,1), SwapTails, Exchange(2,0)
9. `LocalSearch` engine (granular neighbourhood)
10. Construction heuristic (nearest-neighbour or savings)
11. ILS (ruin-and-recreate as regular moves + late acceptance)
12. Warm start: accept initial solution, pin entities
13. Tests: model API, operator correctness, small instance end-to-end

### Phase 2 — Benchmark quality

1. SWAP* operator
2. Exchange(N,M) up to (3,3)
3. PenaltyManager (adaptive α, strategic oscillation)
4. Composable acceptors (late acceptance + tabu)
5. Guided Local Search (GLS: penalize frequent arcs)
6. Weighted operator selection + multi-armed bandit (adaptive weights)
7. Accepted count limit + pick-early termination
8. Move filter interface (user predicates)
9. Diminished returns termination
10. Score corruption detection (debug assertion mode)
11. Score explanation / analysis in result object
12. Benchmarker (parameter grids, multi-seed, score-over-time tracking)
13. Uchoa X-set benchmarks. Target: <2% gap, 60s.

### Phase 3 — HGS + portfolio

1. Population (feasible + infeasible, diversity)
2. Crossover (SREX)
3. `GeneticAlgorithm`
4. Portfolio solving: run ILS + HGS in parallel, shared solution pool
5. Solution finalization (compact waiting times)
6. Compare ILS vs HGS vs portfolio on X-set

### Phase 4 — VRPTW + rich VRP

User can: `m.add_client({.tw = {100,200}, .service = 10}); m.solve();`

Engine:
1. `DurationResource` with `init/merge/merge_reverse/excess`
2. `DistanceResource` for max route distance
3. Route carries multiple resource arrays (load + duration + distance)
4. PenaltyManager adds β (time warp), γ (excess distance)
5. Heterogeneous fleet: fixed cost, variable costs, routing profiles
6. Multi-depot, open routes, site-dependent access
7. Optional clients + prizes, client groups
8. Release times, overtime
9. Multi-dimensional load (N capacity dimensions)
10. Driver breaks (BreakResource: min break duration, max interbreak time)
11. Visit type incompatibilities (hazmat + food can't share vehicle)
12. Visit type requirements (co-presence on same vehicle)
13. LIFO/FIFO pickup-delivery ordering (physical loading constraints)
14. Depot resource constraints (shared loading dock capacity)
15. Global span cost (minimize max route duration across fleet)
16. Solomon + Gehring-Homberger benchmarks

### Phase 4b — Multi-trip

1. Reload-aware route structure (depot visits mid-route)
2. RelocateWithDepot operator
3. LoadResource reset at reload points
4. `max_reloads` constraint

### Phase 4c — Paired pickup-delivery

1. `PrecedenceResource` (pickup before delivery on same route)
2. Pair-aware operators (RelocatePair, SwapPairs)
3. Subtrip operators (RelocateSubtrip, ExchangeSubtrip)
4. LIFO/FIFO loading order support
5. Li-Lim PDPTW benchmarks

### Phase 5 — Scheduling

User can: `m.add_job(); m.add_operation(j, {.machine=0, .duration=3});`

Engine:
1. JSP: disjunctive graph, N5 block moves, tabu search
2. PFSP: permutation representation, NEH construction, insert/swap operators
3. FJSP: machine assignment + sequencing
4. RCPSP: activity list + SGS decoding
5. OSSP: disjunctive graph without intra-job precedence arcs
6. MRCPSP: mode selection (integer variable per activity) + SGS decoding
7. Optional operations: overconstrained scheduling with medium-priority penalty
8. Parallel machine scheduling
9. Taillard + PSPLIB + MMLIB + Guéret-Prins benchmarks

### Phase 6 — Assignment / timetabling

User can: `m.add_employee(...); m.add_shift_type(...); m.solve();`

Engine:
1. `AssignmentModel` class (employees, shifts, constraints, preferences)
2. `AssignmentData` (compiled from model)
3. `AssignmentSolution` (employee → shift assignments per day)
4. Basic operators: ChangeShift, SwapShifts, SwapEmployees
5. Pillar operators: PillarChange, PillarSwap (move groups)
6. Construction: First Fit Decreasing, Cheapest Insertion
7. Constraint evaluation: consecutive shifts, rest hours, weekend balance, skills
8. Load balancing / fairness constraints (workload distribution)
9. Automaton constraint (FSM for forbidden shift sequences: N-N-E forbidden)
10. Hard/soft separation with user-configurable constraint weights
10. Overconstrained support: unassigned entities with medium penalty
11. VND: ordered neighbourhood (reassign → swap → pillar) with reset
12. Tabu search + late acceptance (reuse search control layer)
13. Non-disruptive replanning (penalty for changes from published schedule)
14. INRC-II benchmarks
15. School timetabling (ITC format)

### Phase 6b — Bin packing

User can: `m.add_bin_type(100, {.capacity = 150}); m.add_item({.size = 30});`

Engine:
1. `PackingModel` class (bin types, items, conflicts)
2. `PackingSolution` (item → bin assignments)
3. Operators: MoveItem, SwapItems between bins
4. Per-bin capacity tracking (N dimensions)
5. Conflict constraint (incompatible item pairs)
6. First Fit Decreasing construction
7. Tabu search + late acceptance (reuse search control layer)
8. BPPLIB benchmarks

### Phase 6c — Lot sizing + network flow

1. CLSP/MLCLSP via Fix-and-Optimize with mip-heuristics
2. NetworkModel for MCF / RCMCF
3. Fractional path support (LP / column generation)

### Phase 7 — Advanced search infrastructure

1. Partitioned search (geographic clusters for VRP, department groups for assignment)
2. Daemon mode / continuous solving (problem change queue, real-time dispatch)
3. Non-disruptive replanning for routing (penalty for deviations from published)

### Phase 8 — Extended routing

1. CARP (arc routing via transformation)
2. Electric VRP (BatteryResource + recharging stations)
3. Time-dependent VRP (time-indexed distance functions)
4. Period VRP (multi-day solution structure)
5. Cumulative CVRP (CumulativeCostResource)
6. Clustered VRP (ClusterResource: finish cluster before leaving)
7. VRP with Transshipment (multi-echelon routing via depot-customer assignment)

### Phase 9 — Extended scheduling + packing

1. Preemptive RCPSP (task splitting into subtasks)
2. Car sequencing (permutation + sliding-window ratio constraints)
3. Bin packing with conflicts (BPPC)
4. Piecewise linear cost functions in CostEvaluator

---

## 7. Relationship to mip-heuristics

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
│   move scoring           │          │   resources, operators,  │
│   weight updates         │          │   ILS / HGS, penalties   │
│                          │          │                          │
│ Generic: any MIP         │◄─────────│ Structured: RSP          │
│                          │ Phase 6  │                          │
└──────────────────────────┘          └──────────────────────────┘

Same pattern: user declares WHAT, solver decides HOW.
```

---

## 8. Key Design Decisions

1. **Model is the product.** The user interacts with `Model`, `NetworkModel`,
   `ScheduleModel`, `AssignmentModel`, `PackingModel`, `LotSizingModel`. They
   never see resources, operators, or algorithms.

2. **Attribute-driven engine selection.** When the user sets `tw` on a client,
   the Model activates `DurationResource` internally. No explicit configuration.

3. **Resources as the extensibility mechanism.** A resource is a function on
   subsequences with an associative composition operator. Each one tracks a
   constraint aspect (load, time, pickup state) and reports violations as
   penalties. Adding a new constraint = adding a new C++ resource type with
   `init/merge/merge_reverse/excess`. No changes to operators, algorithms, or
   other resources. The CostEvaluator picks up the new penalty automatically.

4. **Resource independence.** Each resource is self-contained. The CostEvaluator
   sums penalties independently. If two resources genuinely depend on each other
   (e.g., cumulative cost = weight × time), embed both in one resource.

5. **Forward/reverse state asymmetry.** Resources may define different state
   types for forward vs. reverse propagation. Most are symmetric (merge =
   merge_reverse). The interface supports asymmetry when needed (e.g.,
   simultaneous pickup-delivery).

6. **Rich cost model.** Fixed cost + variable distance/duration cost + overtime
   cost + prize collection + penalty terms. Vehicle-type-specific cost parameters.

7. **Multi-dimensional load.** LoadResource supports N capacity dimensions,
   each independently tracked and penalized.

8. **Routing profiles.** Per-vehicle-type distance/duration matrices for
   heterogeneous access or speed.

9. **Fractional paths for network flow.** MCF/RCMCF support flow split across
   paths via LP or column generation. Distinct from VRP (integer routes).

10. **Integer arithmetic in the hot path.** Scale and round distances. CVRPLIB
    uses integers. Avoids floating-point. Cache-friendly.

11. **Granular neighbourhood.** k-nearest (k=40). O(n·k) not O(n²).

12. **Penalized cost.** `α·excess_load + β·time_warp + γ·excess_distance + ...`
    Infeasible intermediate solutions. Adaptive penalty weights targeting a
    configurable feasibility ratio.

13. **Full Exchange(N,M) operator family.** Up to (3,3) for thorough neighbourhood
    exploration, plus SWAP* for efficient inter-route moves.

14. **No ALNS.** ILS and HGS outperform it. ALNS adds complexity without quality.

15. **No abstract Algorithm base.** ILS and HGS are separate classes. Custom
    algorithms use components directly.

16. **Clean function signatures for LLM targeting.** `DestroyFn`, `RepairFn`
    are simple enough for LLM code generation.

17. **Composable acceptors.** Acceptance criteria (late acceptance, SA, tabu) can
    be combined rather than picking one. Avoids single-strategy stagnation.

18. **Ruin-and-recreate as regular moves.** Large-neighbourhood moves go through
    the same acceptance logic as local search moves. No separate ALNS phase.

19. **Warm start and pinning.** Accept user-provided initial solutions. Pinned
    entities are locked during optimization. Essential for re-optimization.

20. **Assignment/timetabling gets native engine.** Employee scheduling, timetabling,
    and similar problems lack sequential structure but have exploitable assignment
    structure. Tabu search + late acceptance with problem-specific moves (swap
    shift, reassign) outperforms generic MIP — same principle as routing.

21. **Benchmarker as first-class tool.** Parameter tuning with grids, multiple
    seeds, score-over-time tracking, and statistical aggregation. Not the solver,
    but essential for development and user experimentation.

22. **Weighted operator selection.** Probability weights on move types, tunable
    per problem type. Avoids wasting time on low-yield operators.

23. **Score corruption detection.** Debug mode recalculates full cost after each
    move, asserts match with incremental. Zero overhead in release builds.

24. **Score explanation in results.** Structured breakdown of which constraints
    are violated by which entities, by how much. Essential for user trust.

25. **Move filters.** User predicates to exclude moves before evaluation.
    Absolute exclusions, not penalized violations. Reduces wasted computation.

26. **Accepted count limit / pick-early.** Tuning knobs for large instances.
    Cap move evaluations per step and/or stop on first improvement.

27. **Non-disruptive replanning.** Penalize deviations from published solution
    when warm-starting. Only change what's worth the disruption.

28. **Overconstrained planning.** Allow unassigned entities with medium-priority
    penalty when not everything can be assigned.

29. **Partitioned search.** Decompose large instances, solve independently, then
    optimize cross-partition. For 5000+ node VRP and large scheduling.

30. **Daemon mode.** Long-running solver accepting problem changes via queue.
    For real-time dispatch and rolling-horizon scheduling.

31. **Packing reuses assignment engine.** Bin packing = items assigned to bins
    with partition constraint implicit. Same engine, different operators (MoveItem,
    SwapItems). Capacity tracked per bin, N dimensions for vector bin packing.

32. **Piecewise linear cost functions.** Tiered pricing, volume discounts,
    multi-tier overtime defined via breakpoints. Lookup table in CostEvaluator.

33. **Depth over breadth.** We provide specialized engines (routing, scheduling,
    assignment, packing) with hand-tuned algorithms per problem type. Hexaly's
    approach is general-purpose (one solver, many problem types). We will never
    match their breadth, but we outperform on the problems we cover.

34. **Guided Local Search.** GLS penalizes frequently-used solution features
    (arcs, assignments) to escape local optima. Complementary to strategic
    oscillation: GLS diversifies by feature frequency, oscillation by feasibility.

35. **Multi-armed bandit operator selection.** Adaptive weights learned online
    via UCB or Thompson Sampling. Replaces static probability weights when
    enabled. Learns which operators are productive per instance.

36. **Real-world routing constraints.** Driver breaks, type incompatibilities,
    type requirements, LIFO/FIFO loading, depot resources, global span cost.
    These come from OR-Tools' decade of production use, not academic benchmarks.

37. **Portfolio solving.** Run ILS + HGS + SA in parallel with shared solution
    pool. Cross-pollination via crossover restarts. Complementary search
    strategies strictly better than multi-start within one algorithm.

38. **Solution finalization.** Post-optimization pass to compact secondary
    variables (waiting times, idle gaps) without changing structure.

39. **Automaton constraint for shift patterns.** Finite state machine defining
    forbidden/required shift sequences (e.g., no Night-Night-Early). Expressive
    and efficient for nurse rostering pattern rules.

---

## 9. References

Core methods:
- Vidal et al. (2014). *A unified solution framework for multi-attribute VRPs*. C&OR.
- Vidal (2022). *HGS for the CVRP: SWAP\**. C&OR.
- Wouda et al. (2024). *PyVRP: a high-performance VRP solver package*. IJOC.
- Maximo et al. (2024). *AILS-II: Adaptive ILS for large-scale CVRP*. IJOC.

Resource modeling (design influence):
- Sadykov, Uchoa et al. (2026). *Bucket Graph Meta-Solver for RCSP*. HAL-05486295.

Benchmarks:
- Uchoa et al. (2017). *New benchmark instances for the CVRP*. EJOR.
- CVRPLIB BKS Challenge (2026). https://vrp.galgos.inf.puc-rio.br/
- CVRPLIB. https://galgos.inf.puc-rio.br/cvrplib/
- SINTEF TOP. https://www.sintef.no/projectweb/top/
- PyVRP/Instances. https://github.com/PyVRP/Instances
- DIMACS 12th Challenge. http://dimacs.rutgers.edu/programs/challenge/vrp/
- JSPLIB. https://github.com/tamy0612/JSPLIB
- FJSPLIB. https://scheduleopt.github.io/benchmarks/fjsplib
- PSPLIB. https://www.om-db.wi.tum.de/psplib/
- VFR (flow shop). http://soa.iti.es/problem-instances

Scheduling:
- Nowicki & Smutnicki (1996). *A fast taboo search for the job shop*. MS.
- Taillard (1993). *Benchmarks for basic scheduling problems*. EJOR.

Lot sizing:
- Helber & Sahling (2010). *Fix-and-optimize for CLSP*. IJPE.
- Muller, Spoorendonk & Pisinger (2012). *Hybrid ALNS for lot-sizing*. EJOR.

Solver design influence:
- Timefold Solver. Composable acceptors, multi-level scoring, warm start/pinning,
  diminished returns termination, benchmarker, weighted operator selection, move
  filters, score corruption detection, score explanation, non-disruptive
  replanning, overconstrained planning, VND, pillar moves, partitioned search,
  daemon mode, construction heuristics (FFD, cheapest insertion), tabu search
  for assignment/timetabling. https://github.com/TimefoldAI/timefold-solver
- Hexaly (formerly LocalSolver). Set/list variables, piecewise linear costs,
  bin packing via set-partition, multi-mode RCPSP, open shop, clustered VRP,
  VRP with transshipment, car sequencing. https://www.hexaly.com/
- Google OR-Tools. RoutingModel dimensions, driver breaks, type incompatibilities,
  LIFO/FIFO PD, depot resources, GLS, MAB operator selection, global span,
  portfolio solving, solution finalization, automaton constraint.
  https://github.com/google/or-tools

LLM/Neural:
- Ye et al. (2025). *VRPAgent: LLM-driven operator discovery*. arXiv.
- Liu et al. (2024). *Evolution of Heuristics (EoH)*. ICML.
- Romera-Paredes et al. (2024). *FunSearch*. Nature.
