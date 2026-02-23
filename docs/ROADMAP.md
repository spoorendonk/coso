# COSO — Combinatorial Structure-aware Optimization

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

### 1.7 Job shop scheduling — same pattern, different domain

```cpp
#include <coso/scheduling.h>

coso::ScheduleModel m;

// Jobs with ordered operations
auto j1 = m.add_job();
m.add_operation(j1, {.machine = 0, .duration = 3});
m.add_operation(j1, {.machine = 1, .duration = 2});

auto j2 = m.add_job();
m.add_operation(j2, {.machine = 1, .duration = 4});
m.add_operation(j2, {.machine = 0, .duration = 1});

m.minimize_makespan();
auto result = m.solve(coso::TimeLimit(30));
```

Or from a standard format:

```cpp
auto result = coso::solve_jsp("tai20x15.txt", coso::TimeLimit(30));
```

### 1.8 Nurse rostering — assignment with tabu search

```cpp
#include <coso/assignment.h>

coso::AssignmentModel m;

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
m.add_constraint(coso::MaxConsecutiveShifts(5));
m.add_constraint(coso::MinRestBetweenShifts(11));  // hours
m.add_constraint(coso::MaxNightShiftsPerWeek(2));
m.add_constraint(coso::WeekendBalancing());

// Preferences (soft constraints)
alice.prefer_off({5, 12, 19, 26});  // Saturdays off
bob.prefer_shift(early);             // prefers early shifts

auto result = m.solve(coso::TimeLimit(60));

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

### Network / Flow

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| Arcs + commodities + demands | Multi-commodity flow | Network simplex / CG |
| Arc resources + bounds | Resource-constrained MCF | Column generation + RCSPP |
| Ports + vessels + demands + schedules | Liner shipping network design | Routing + scheduling + fleet assignment |

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
| Multiple (machine, duration) per op | Alternative / mode selection | ChangeMode + ReassignMachine |

### Assignment / Timetabling

| User declares | Model recognizes | Engine maps to |
|---|---|---|
| Employees + shifts + constraints | Nurse rostering / employee scheduling | Assignment engine (tabu + VND) |
| Employees + activities + days | Multi-activity multi-day scheduling | Assignment engine (tabu + VND) |
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

The user never sees this. The model translates their declaration into engine
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
3. Registers the resource so the model activates it when relevant attributes
   are declared

No changes to operators, local search, algorithms, or other resources. The
CostEvaluator picks up the new penalty term automatically.

**Dogfooding: built-in resources use the same interface.** LoadResource,
DurationResource, DistanceResource — all built-in resources are implemented
through the same `init/merge/merge_reverse/excess` interface as user-defined
resources. If you wanted to reimplement capacity tracking differently, you'd
write a new resource file with the same interface and swap it in. Exception:
if the generic interface introduces measurable overhead for a hot-path
resource, a specialized fast path is allowed — but this should be the
exception, not the rule, and the generic interface must still work.

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

**Three-matrix cost model.** Per profile, three separate matrices: duration
(for feasibility/scheduling), distance (for reporting/max-distance), cost (for
optimization). `cost ≠ distance` when toll roads add cost but not time.
`cost ≠ duration` when driver wages differ from fuel cost. Per-task-hour cost
separates time spent serving from time spent traveling.

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
| RouteSplit | Split overloaded route across empty vehicles |

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

**Conflict-directed move selection.** Focus moves on the earliest/worst conflict
point rather than random selection. For routing: concentrate operators around
the highest-violation route segment. For scheduling: target the earliest
overlapping pair on a machine. Improves convergence by directing search effort
where it matters most.

**Scheduling-specific perturbation operators.** From CP-SAT's LNS framework:
- Time-window relaxation: fix tasks before T and after T+W, free the middle
- Precedence relaxation: keep ordering decisions, free timing
- Per-resource relaxation: free all tasks on one machine, re-optimize
- Adaptive difficulty: track acceptance rates per operator, adjust perturbation
  size (fraction of variables freed) based on success rate

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

**Score corruption detection.** Debug mode (`COSO_ASSERT_SCORES`) that
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

### 3.8 CP-as-move-filter for assignment/packing engines

Inspired by JuLS (Amazon): use lightweight constraint propagation to **prune
infeasible moves before scoring**. The search loop becomes:

```
Assignment/Packing engine search loop:
  1. Generate candidate moves (swap, reassign, chain, pillar)
  2. CP filter: propagate constraints, prune infeasible moves
  3. Score remaining moves via incremental invariants
  4. Accept/reject via metaheuristic (tabu + LA)
```

**Why only assignment/packing, not routing/scheduling?** Route and schedule
feasibility is already checked in O(1) via resource excess — faster than any CP
propagator. Assignment problems lack this sequential structure; their constraints
(forbidden sequences, cardinality, coverage, skill matching) benefit from domain
filtering.

Propagators are lightweight — not a full CP solver, just constraint-specific
filters:

| Propagator | Constraint | Effect |
|---|---|---|
| ForbiddenSequence | No night→early | Prunes shift assignments that create violations |
| Cardinality | Min/max employees per shift | Prunes moves that under/overstaff |
| AllDifferent | No duplicate assignments | Prunes conflicting swaps |
| SkillCoverage | Required skills per shift | Prunes moves that lose required coverage |
| BinCapacity | Item fits in bin | Prunes infeasible item-to-bin assignments |
| Conflict | Incompatible items | Prunes co-placement of conflicting items |

Move filtering rate depends on constraint tightness — nurse rostering with tight
labor rules can filter 40-60% of candidate moves, avoiding wasted evaluation.

The routing/scheduling engines skip this layer entirely; their resource system
already provides O(1) feasibility checking which is strictly faster.

### 3.9 LLM-friendly operator interfaces

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
| R27 | Technician Routing & Scheduling | TRSP | Skills + TW + teams | 5 | ROADEF 2007, Solomon-TRSP | 36+ | HAL/ROADEF |
| R28 | Home Healthcare Routing | HHCRP | Skills + TW + sync | 5+ | Mankowska et al. | varies | Papers |

### 4.2 Network / flow problems

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
| A1 | Nurse Rostering | NRP | Assignment engine (tabu + LA) | 8 | INRC-I, INRC-II, BCV, GPost, SINTEF, ORTEC, Ikegami, Montreal | 100+ | INRC, schedulingbenchmarks.org |
| A2 | Employee Scheduling | ESP | Assignment engine (tabu + LA) | 8 | Curtois shift scheduling (instances 1-24) | 24 | schedulingbenchmarks.org |
| A3 | Multi-Activity Scheduling | MATSP | Assignment engine (tabu + LA) | 8 | Curtois multi-activity multi-day | 225 | schedulingbenchmarks.org |
| A4 | School Timetabling | — | Assignment engine (tabu + LA) | 8+ | ITC-2007, ITC-2019 | varies | ITC |
| A5 | Conference Scheduling | — | Assignment engine (tabu + LA) | 8+ | — | — | — |
| A6 | Bed Allocation | BAS | Assignment engine (tabu + LA) | 8+ | Ceschia-Schaerf | 15+ | Papers |

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
| ROADEF 2007 | TRSP | Yes (Phase 5) |
| BCV/XML (schedulingbenchmarks.org) | NRP | Yes (Phase 8) |
| Curtois shift scheduling | ESP | Yes (Phase 8) |
| Curtois multi-activity | MATSP | Yes (Phase 8) |
| LINERLIB | LSNDP | Later |

### 4.8 What the modeling approach handles well vs. not

**Handles well (resource pattern):** Problems where the solution is a set of
sequences (routes, job orderings) and constraints can be evaluated by propagating
state along each sequence. R1–R15, R17, R20, R24–R28, S1–S12.

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

**Network / fractional:** MCF, RCMCF, and liner shipping (N1–N3) support
fractional paths and network-level design. Liner shipping (N3) is an integrated
routing + scheduling + fleet assignment problem — routes are cyclic services
with weekly schedules, combining network design with vehicle deployment.
Uses LP / column generation rather than local search.

**Assignment / timetabling (structure-aware, not MIP).** Problems like nurse
rostering, employee scheduling, multi-activity scheduling, timetabling, and
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
  model/              ← User-facing modeling API (C++ headers + Python via nanobind)
    types.h               Shared types (Coord, TimeWindow, CostParams, Result)
    routing_model.h       Routing model (add_depot, add_client, add_vehicle_type)
    schedule_model.h      Scheduling model (add_job, add_operation)
    assignment_model.h    Assignment model (add_employee, add_shift_type)
    packing_model.h       Packing model (add_bin_type, add_item)
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
      skill_filter.h          Skills matching (precomputed compatibility)
      task_count_resource.h   Max tasks per vehicle
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
    perturbation/
      time_window_relax.h   Fix before T, free middle, fix after T+W
      precedence_relax.h    Keep ordering, free timing
      resource_relax.h      Free all tasks on one machine, re-optimize
      change_mode.h         Switch activity to different (machine, duration) mode
      reassign_machine.h    Move task to different machine (FJSP)

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

python/               ← nanobind Python bindings
  bindings.cpp            Bind all model types + Result + solve()
  pyproject.toml          scikit-build-core config for pip install

tests/
  model/              ← User-facing tests (the API contract)
  routing/
  scheduling/
  assignment/
  packing/
  search/
  data/               ← Benchmark instances (downloaded, not in git)
```

---

## 6. Preliminary Modeling Interface

C++ headers defining the public API. No implementation yet — these pin down what
the user sees before we write any solver code. Python bindings (nanobind) will
mirror these interfaces 1:1.

### 6.1 Shared types (`src/model/types.h`)

```cpp
#pragma once
#include <string>
#include <vector>

namespace coso {

struct Coord { double x, y; };

struct TimeWindow { int start, end; };

struct CostParams {
    int fixed_cost = 0;
    int unit_distance_cost = 1;
    int unit_duration_cost = 0;
    int per_task_hour_cost = 0;
};

/// Stop criterion passed to solve().
struct TimeLimit {
    double seconds;
    explicit TimeLimit(double s) : seconds(s) {}
};

/// Common result fields shared across all engines.
/// All fields use underscore-suffixed storage with [[nodiscard]] accessors.
/// Engine-specific accessors (only populated by the relevant engine):
///   routing:    routes(), unserved()
///   scheduling: makespan(), schedule()
///   assignment: assignments(), day(), unassigned()
///   packing:    bins(), num_bins()
///   network:    flows()
struct Result {
    bool   feasible_        = false;
    double cost_            = 0.0;
    double elapsed_seconds_ = 0.0;
    int    iterations_      = 0;

    [[nodiscard]] bool   feasible()        const noexcept;
    [[nodiscard]] double cost()            const noexcept;
    [[nodiscard]] double elapsed_seconds() const noexcept;
    [[nodiscard]] int    iterations()      const noexcept;
    // ... plus engine-specific accessors (see src/model/types.h)
};

} // namespace coso
```

### 6.2 Routing model (`src/model/routing_model.h`)

```cpp
#pragma once
#include "types.h"

namespace coso {

struct VehicleTypeParams {
    std::vector<int> capacity;     // N dimensions
    int max_duration = 0;          // 0 = unlimited
    int max_distance = 0;
    int max_tasks = 0;
    CostParams cost;
    int profile = 0;               // distance/duration matrix index
    double speed_factor = 1.0;
    std::vector<std::string> skills;
};

struct ClientParams {
    std::vector<int> demand;       // N dimensions (matches capacity)
    std::vector<int> pickup;       // backhaul pickup
    TimeWindow tw = {0, INT_MAX};
    std::vector<TimeWindow> extra_tw; // additional time windows
    int service = 0;
    int release_time = 0;
    int prize = 0;                 // for optional clients
    bool required = true;
    int group = -1;                // client group (-1 = none)
    std::vector<std::string> skills;
    int setup_time = 0;
    int location = -1;             // for location-aware setup: skip if same
};

struct DepotParams {
    TimeWindow tw = {0, INT_MAX};
};

class RoutingModel {
public:
    int add_depot(double x, double y, DepotParams p = {});
    int add_depot(int id, DepotParams p = {});  // when using explicit distances

    int add_vehicle_type(int count, VehicleTypeParams p = {});

    int add_client(double x, double y, ClientParams p = {});
    int add_client(int id, ClientParams p = {});

    // Pickup-delivery pairs
    void add_pickup_delivery(int pickup, int delivery);

    // Distance/duration matrices
    void set_distance(int from, int to, int dist);
    void set_duration(int from, int to, int dur);
    void set_profile_distance(int profile, int from, int to, int dist);
    void set_profile_duration(int profile, int from, int to, int dur);
    void set_cost_matrix(int profile, int from, int to, int cost);

    // Warm start
    void set_initial_routes(const std::vector<std::vector<int>>& routes);
    void pin(int client_id);  // lock in current position during re-optimization

    Result solve(TimeLimit tl);
};

// Convenience: solve from CVRPLIB/VRPLIB file
Result solve(const std::string& instance_path, TimeLimit tl);

} // namespace coso
```

### 6.3 Scheduling model (`src/model/schedule_model.h`)

```cpp
#pragma once
#include "types.h"

namespace coso {

struct MachineParams {
    std::string name;
};

struct OperationParams {
    int machine = -1;                          // -1 = flexible (FJSP)
    std::vector<int> eligible_machines;        // for FJSP: machine alternatives
    std::vector<int> durations_per_machine;    // duration on each eligible machine
    int duration = 0;                          // fixed duration (when machine fixed)
    bool optional = false;
};

struct JobParams {
    std::string name;
    int release_time = 0;
    int due_date = INT_MAX;
    int weight = 1;          // for weighted tardiness
};

enum class ScheduleObjective {
    Makespan,                // minimize max completion time
    TotalWeightedTardiness,  // minimize Σ w_j * max(0, C_j - d_j)
    TotalFlowTime,           // minimize Σ C_j
};

class ScheduleModel {
public:
    int add_machine(MachineParams p = {});
    int add_job(JobParams p = {});
    int add_operation(int job, OperationParams p);

    // Resource constraints (RCPSP)
    int add_resource(int capacity);
    void set_resource_usage(int operation, int resource, int amount);

    // Precedence (beyond default intra-job ordering)
    void add_precedence(int op_before, int op_after);

    void set_objective(ScheduleObjective obj);

    // Warm start
    void set_initial_schedule(/* operation → (machine, start_time) */);

    Result solve(TimeLimit tl);
};

} // namespace coso
```

### 6.4 Assignment model (`src/model/assignment_model.h`)

```cpp
#pragma once
#include "types.h"

namespace coso {

struct ShiftTypeParams {
    std::string name;
    int start_hour = 0;
    int end_hour = 8;
    int duration_hours = 0;  // 0 = computed from start/end
};

struct EmployeeParams {
    std::string name;
    std::vector<std::string> skills;
    int max_hours_per_week = 40;
    int max_consecutive_days = 5;
    int min_rest_hours = 11;
};

struct DemandParams {
    int min_employees = 0;
    int max_employees = INT_MAX;
    std::string required_skill;  // empty = no skill requirement
};

class AssignmentModel {
public:
    int add_shift_type(ShiftTypeParams p);
    int add_employee(EmployeeParams p);

    void set_horizon(int days);

    // Demand: day × shift → min/max employees
    void add_demand(int shift_type, int day, DemandParams p);
    void add_demand(int shift_type, DemandParams p);  // all days

    // Constraints (built-in)
    void set_max_consecutive_shifts(int n);
    void set_min_rest_between_shifts(int hours);

    // Automaton constraint for forbidden/required shift sequences
    void add_forbidden_sequence(const std::vector<int>& shift_types);

    // Preferences (soft)
    void add_preference(int employee, int day, int shift_type, int weight);
    void add_unavailability(int employee, int day);

    // Warm start / replanning
    void set_published_schedule(/* employee × day → shift */);
    void set_change_penalty(int penalty);  // cost per deviation from published

    Result solve(TimeLimit tl);
};

} // namespace coso
```

### 6.5 Packing model (`src/model/packing_model.h`)

```cpp
#pragma once
#include "types.h"

namespace coso {

struct BinTypeParams {
    std::vector<int> capacity;   // N dimensions
    int cost = 1;                // cost per bin used
    int count = 0;               // 0 = unlimited
};

struct ItemParams {
    std::vector<int> size;       // N dimensions (matches capacity)
};

class PackingModel {
public:
    int add_bin_type(BinTypeParams p);
    int add_item(ItemParams p);

    // Constraints
    void add_conflict(int item_a, int item_b);  // cannot share a bin

    // Objective: minimize total bin cost (default)
    void minimize_bins();

    Result solve(TimeLimit tl);
};

} // namespace coso
```

---

## 7. Implementation Roadmap

Each **work unit** (e.g., 1.1, 2.3, 5.7) is one PR. When you say "do 2.3",
that means: create branch `2.3-route-load-resource`, implement it, open a PR.

Work units have explicit file ownership and dependencies. Agents check open
branches and PRs before claiming a unit.

"Extract don't abstract" — shared infrastructure emerges from routing, then
gets reused by later engines.

### Step 1 — Repo skeleton + model headers

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 1.1 | CMake + CI setup | Project compiles, `ctest` runs (0 tests) | CMakeLists.txt, cmake/, .github/ | — |
| 1.2 | Shared types header | `types.h` with Coord, TimeWindow, CostParams, Result, TimeLimit | src/model/types.h | Done |
| 1.3 | Model headers (declarations only) | All 4 model classes declared, compile with no impl | src/model/routing_model.h, schedule_model.h, assignment_model.h, packing_model.h | Done |
| 1.4 | API contract tests | Tests that exercise model API (compile + link, assert on trivial cases) | tests/model/model_test.cpp | Done |

1.1 and 1.2 are parallel. 1.3 needs 1.2. 1.4 needs 1.1 + 1.3.

### Step 2 — CVRP end-to-end

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 2.1 | ~~CVRPLIB instance reader~~ | ~~Parse .vrp files into structured data~~ | ~~src/model/instance_reader.{h,cpp}, tests~~ | ~~1.2~~ |
| 2.2 | ProblemData (compiled instance) | Distance matrix, client/depot/vehicle attributes from model | src/routing/problem_data.{h,cpp}, tests | Done |
| 2.3 | ~~Route + LoadResource~~ | ~~Route with prefix/suffix resource arrays, LoadResource with init/merge/excess~~ | ~~src/routing/route.{h,cpp}, src/routing/resources/load_resource.h, tests~~ | ~~Done~~ |
| 2.4 | ~~Solution + CostEvaluator~~ | ~~Multi-route solution, objective + penalty evaluation~~ | ~~src/routing/solution.{h,cpp}, src/routing/cost_evaluator.{h,cpp}, tests~~ | ~~Done~~ |
| 2.5 | Exchange operators | Exchange(1,0), (1,1), (2,0), SwapTails with O(1) move eval | src/routing/operators/exchange.{h,cpp}, tests | 2.3, 2.4 |
| 2.6 | Local search engine | Granular neighbourhood (k=40), steepest descent over operators | src/routing/local_search.{h,cpp}, src/routing/neighbours.{h,cpp}, tests | 2.5 |
| 2.7 | ~~Construction heuristic~~ | ~~Nearest-neighbour + Clarke-Wright savings~~ | ~~src/routing/construction.{h,cpp}, tests~~ | ~~Done~~ |
| 2.8 | ILS + stop criterion | Ruin-and-recreate + late acceptance, time/iter/no-improve limits | src/search/iterated_local_search.{h,cpp}, src/search/stop_criterion.{h,cpp}, tests | 2.6, 2.7 |
| 2.9 | RoutingModel implementation | Model → ProblemData → construct → ILS → Result | src/model/routing_model.cpp, tests | 2.2, 2.8 |
| 2.10 | CLI | `coso-solve instance.vrp --time-limit 60` | src/cli/main.cpp | 2.9 |
| 2.11 | Benchmark setup + first benchmarks | Download script, X-n101-k25 end-to-end test | tests/data/download_benchmarks.sh, tests/routing/benchmark_test.cpp | 2.1, 2.9 |

**Parallel lanes:**
- 2.1 and 2.2 are parallel (both only need 1.2)
- 2.7 is parallel with 2.5/2.6 (both need 2.4, merge at 2.8)

### Step 3 — Routing benchmark quality

```
Deliverable: <2% gap on Uchoa X-set in 60s. Competitive with PyVRP.
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 3.1 | SWAP* operator | SWAP* move with O(1) eval via best-insert cache | src/routing/operators/swap_star.{h,cpp} | 2.5 |
| 3.2 | Extended exchange family | Exchange(N,M) for N,M up to 3 | src/routing/operators/exchange.{h,cpp} (extend) | 2.5 |
| 3.3 | Adaptive penalty manager | Auto-tune capacity/TW penalty weights per iteration | src/search/penalty_manager.{h,cpp} | 2.8 |
| 3.4 | Composable acceptance criteria | Late acceptance, simulated annealing, record-to-record | src/search/acceptance.{h,cpp} | 2.8 |
| 3.5 | Guided local search | GLS with edge penalties for diversification | src/search/guided_local_search.{h,cpp} | 2.6 |
| 3.6 | Operator selector (MAB) | Multi-armed bandit for operator selection | src/search/operator_selector.{h,cpp} | 2.8 |
| 3.7 | Score corruption detection | Debug assertions verifying incremental vs full recompute | src/search/score_assert.{h,cpp} | 2.4 |
| 3.8 | Score explanation | Human-readable cost breakdown for debugging | src/search/score_analysis.{h,cpp} | 2.4 |
| 3.9 | Benchmark harness | Automated benchmark runner with CSV output + gap reporting | src/search/benchmarker.{h,cpp} | 2.11 |

**All of 3.1–3.9 are parallel** — they touch different files. Integrate
together at the end for benchmark runs.

### Step 4 — HGS + portfolio

```
Deliverable: portfolio solver (ILS + HGS) with shared solution pool.
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 4.1 | Population with diversity management | Biased fitness with broken-pairs diversity | src/search/population.{h,cpp} | 2.4 |
| 4.2 | SREX crossover | Selective route exchange crossover operator | src/search/crossover.{h,cpp} | 2.4 |
| 4.3 | Genetic algorithm | HGS-style GA: select parents → crossover → educate → insert | src/search/genetic_algorithm.{h,cpp} | 4.1, 4.2, 2.6 |
| 4.4 | Portfolio solver | ILS + HGS with shared solution pool, TBB parallel | src/search/portfolio.{h,cpp} | 4.3, 2.8 |
| 4.5 | Solution finalizer | Post-optimization: inter-route moves at zero penalty | src/search/solution_finalizer.{h,cpp} | 2.4 |

4.1, 4.2, 4.5 are parallel. 4.3 merges 4.1+4.2. 4.4 merges everything.

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
| 5.7 | Depot resource | Multi-depot assignment with open/close times | src/routing/resources/depot_resource.h | 2.3 |
| 5.8 | Task count resource | Min/max clients per route | src/routing/resources/task_count_resource.h | 2.3 |

**Operators + features (parallel where noted):**

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 5.9 | Pair operators | Relocate-pair, swap-pair for pickup-delivery | src/routing/operators/pair_operators.{h,cpp} | 5.3 |
| 5.10 | Multi-trip support | Relocate-with-depot-insert for multi-trip VRP | src/routing/operators/relocate_with_depot.{h,cpp} | 2.5 |
| 5.11 | Route split operator | Split long routes at optimal point | src/routing/operators/route_split.{h,cpp} | 2.5 |
| 5.12 | Optional client handling | Insert/remove operators for optional visits | src/routing/operators/insert_optional.{h,cpp} | 2.5 |
| 5.13 | Warm start + pinning | Initialize from existing solution, pin fixed clients | src/search/warm_start.{h,cpp} | 2.8 |
| 5.14 | Rich VRP instance parsers | Solomon, Li-Lim, Gehring-Homberger parsers | src/model/instance_reader.cpp (extend) | 2.1 |
| 5.15 | VRPTW benchmarks | Solomon C1/R1/RC1 end-to-end gap tests | tests/routing/ (extend) | 5.1, 5.14 |

**5.1–5.8 are all parallel.** 5.9–5.13 are parallel. 5.14 is independent.

### Step 6 — Python bindings

```
Deliverable: `pip install coso`, Python API mirrors C++.
Touches only python/ directory — can run parallel with step 5 resources.
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 6.1 | nanobind + scikit-build-core setup | `pip install -e .` builds extension module | python/bindings.cpp, pyproject.toml, CMakeLists.txt (extend) | 2.9 |
| 6.2 | RoutingModel Python bindings | `coso.RoutingModel` mirrors C++ API | python/bindings.cpp (routing section) | 2.9 |
| 6.3 | Python test suite | pytest tests for model API + solve + result access | python/tests/ | 6.2 |

### Step 7 — Scheduling engine

```
Deliverable: solve JSP, FJSP, RCPSP from ScheduleModel API.
Touches only src/scheduling/ and src/model/schedule_model.cpp — fully parallel
with step 5 (rich VRP) and step 8 (assignment).
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 7.1 | ScheduleModel implementation | Model → compiled instance for scheduling engine | src/model/schedule_model.cpp | 1.3 |
| 7.2 | Disjunctive graph data structure | DAG with machine cliques, critical path computation | src/scheduling/disjunctive_graph.{h,cpp} | — |
| 7.3 | Schedule solution representation | Start times, makespan, Gantt-chart output | src/scheduling/schedule_solution.{h,cpp} | 7.2 |
| 7.4 | Schedule operators | N5/N7 neighbourhood: swap, insert, block moves | src/scheduling/schedule_operators.{h,cpp} | 7.3 |
| 7.5 | Construction heuristics (NEH, SGS) | Priority-rule SGS for RCPSP, NEH for flow shop | src/scheduling/construction.{h,cpp} | 7.3 |
| 7.6 | Mode selection for RCPSP | Multi-mode resource assignment with greedy + local search | src/scheduling/mode_selection.{h,cpp} | 7.3 |
| 7.7 | Scheduling perturbation | Ruin-and-recreate for scheduling (random block removal) | src/scheduling/perturbation/ | 7.4 |
| 7.8 | Scheduling instance parsers | Taillard, PSPLIB, FJSP parsers | src/scheduling/parsers/ | 7.1 |
| 7.9 | Scheduling benchmarks | Taillard JSP + PSPLIB RCPSP gap tests | tests/scheduling/ | 7.1, 7.4 |

**7.2, 7.5, 7.6, 7.8 are parallel.** Integration merges at 7.9.

### Step 8 — Assignment / timetabling engine

```
Deliverable: solve nurse rostering, timetabling from AssignmentModel API.
Touches only src/assignment/ and src/model/assignment_model.cpp — fully
parallel with step 7 (scheduling).
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 8.1 | AssignmentModel implementation | Model → compiled instance for assignment engine | src/model/assignment_model.cpp | 1.3 |
| 8.2 | Assignment data + solution | Shift/slot matrix, employee-day assignments, cost eval | src/assignment/assignment_data.{h,cpp}, assignment_solution.{h,cpp} | 8.1 |
| 8.3 | Basic assignment operators | Swap-shift, move-shift, swap-block between employees | src/assignment/operators/ | 8.2 |
| 8.4 | Pillar operators | Multi-employee column moves (VND-style) | src/assignment/operators/pillar_*.{h,cpp} | 8.2 |
| 8.5 | Construction heuristic (FFD) | First-fit-decreasing for initial feasible roster | src/assignment/construction.{h,cpp} | 8.2 |
| 8.6 | Constraint evaluation framework | Incremental soft/hard constraint delta computation | src/assignment/constraints/ | 8.2 |
| 8.7 | Automaton constraint | DFA-based shift pattern rules (e.g., no 3 nights) | src/assignment/constraints/automaton.{h,cpp} | 8.6 |
| 8.8 | CP move filter | Constraint propagation to prune infeasible moves | src/assignment/cp_filter.{h,cpp} | 8.6 |
| 8.9 | Assignment instance parsers | NRP, XML roster format parsers | src/assignment/parsers/ | 8.1 |
| 8.10 | Assignment benchmarks | schedulingbenchmarks.org NRP gap tests | tests/assignment/ | 8.3, 8.9 |

**8.3, 8.4, 8.5, 8.6, 8.9 are parallel.** 8.7 and 8.8 depend on 8.6.

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

### Step 10 — Advanced features

```
Deliverable: production-ready features across all engines.
Each work unit is independent — max parallelism.
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 10.1 | Partitioned search | Decompose large instances into sub-problems | src/search/partitioned_search.{h,cpp} | 2.6 |
| 10.2 | Daemon mode | Continuous solving with dynamic updates | src/search/daemon.{h,cpp} | 2.8 |
| 10.3 | Routing replanning | Re-optimize with fixed/pinned clients, warm start | src/search/warm_start.cpp (extend) | 5.13 |
| 10.4 | Assignment replanning | Re-roster with locked shifts and new constraints | src/assignment/ (extend) | 8.3 |
| 10.5 | Overconstrained handling | Soft violations with cost penalties for infeasible instances | src/routing/, src/assignment/ (extend) | 2.4, 8.2 |
| 10.6 | Piecewise linear costs | Non-linear distance/duration cost functions | src/routing/cost_evaluator.cpp (extend) | 2.4 |
| 10.7 | Extended routing resources | Compartments, loading constraints, sync visits | src/routing/resources/ (new resources) | 2.3 |
| 10.8 | Extended scheduling | Setup times, sequence-dependent setups, calendars | src/scheduling/ (extend) | 7.4 |
| 10.9 | Lot sizing engine | CLSP/MLCLSP, delegates to mip-heuristics | src/lotsizing/ | 1.2 |
| 10.10 | Network flow engine | MCF/RCMCF, column generation for liner shipping | src/network/ | 1.2 |

### Parallelism summary

```
Step 1: sequential (foundation)
Step 2: 2 parallel lanes (data + construction), then integration
Step 3: 9 parallel work units (all touch different files)
Step 4: 3 parallel, then merge
Step 5: 8 resources parallel + 5 operators parallel + parser independent
Step 6: parallel with steps 5, 7, 8, 9 (only touches python/)
Step 7: parallel with steps 5, 8, 9 (only touches src/scheduling/)
Step 8: parallel with steps 5, 7, 9 (only touches src/assignment/)
Step 9: parallel with steps 7, 8 (only touches src/packing/)
Step 10: all 10 work units parallel
```

**Agent coordination rule**: before starting work, check open branches and PRs.
Each work unit ID (e.g., "5.3") maps to a branch name (`5.3-precedence-resource`).
Never start a work unit that another agent has an open branch for.

### On shared infrastructure timing

The "extract don't abstract" principle: build routing (step 2-5), then when
building scheduling (step 7) extract what's genuinely shared. Don't pre-build
abstractions for engines that don't exist yet.

What's obviously shared from day one (step 1):
- `types.h` — Coord, TimeWindow, CostParams, Result, TimeLimit
- CMake/test infrastructure
- Model header pattern

What emerges during routing (step 2-4) and gets reused:
- `search/` — ILS, acceptance criteria, penalty manager, operator selector
- `search/stop_criterion.h` — time/iteration/no-improve limits
- Solution pool, TBB parallel infrastructure
- Score corruption detection, score explanation

What does NOT get pre-built:
- Abstract "Engine" base class — each engine is different
- Generic "Move" type — routing moves ≠ scheduling moves ≠ assignment moves
- Shared "Resource" interface — resources are engine-specific

---

## 8. Relationship to mip-heuristics

```
mip-heuristics                        coso
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
│ Generic: any MIP         │◄─────────│ Structured: CO           │
│                          │ Phase 6  │                          │
└──────────────────────────┘          └──────────────────────────┘

Same pattern: user declares WHAT, solver decides HOW.
```

---

## 9. Key Design Decisions

1. **Model is the product.** The user interacts with `RoutingModel`,
   `ScheduleModel`, `AssignmentModel`, `PackingModel`. They never see
   resources, operators, or algorithms.

2. **Attribute-driven engine selection.** When the user sets `tw` on a client,
   the RoutingModel activates `DurationResource` internally. No explicit configuration.

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

40. **Skills matching.** `job.skills ⊆ vehicle.skills`. Precomputed compatibility
    matrix eliminates infeasible moves before evaluation. Most-requested
    production routing feature.

41. **Setup times (location-aware).** Setup time applied only on location change.
    Skip if consecutive jobs at same location. Realistic for parking/docking.

42. **Multiple time windows.** Array of [start,end] per client instead of single
    TW. "Deliver 9-12 OR 14-17" is standard in last-mile delivery.

43. **Three-matrix cost model.** Per profile: duration (scheduling), distance
    (reporting), cost (optimization). Toll roads add cost but not time.

44. **Scheduling-specific LNS.** Time-window relaxation, precedence relaxation,
    per-resource relaxation from CP-SAT. These outperform generic ruin-and-
    recreate for scheduling because they exploit problem structure.

45. **Conflict-directed move selection.** Focus moves on worst violation point
    rather than random. Earliest overlapping pair (scheduling), highest-
    violation segment (routing). Improves convergence.

46. **Adaptive perturbation difficulty.** Track acceptance rates per operator
    and adjust perturbation size. Successful operators get larger
    neighborhoods; struggling operators shrink.

47. **CP-as-move-filter for assignment/packing.** Lightweight constraint
    propagation prunes infeasible candidate moves *before* scoring (JuLS
    pattern). Forbidden-sequence, cardinality, skill-coverage, and conflict
    propagators filter 40-60% of moves in tightly constrained problems.
    Routing/scheduling engines skip this — their resource system already
    provides O(1) feasibility. This is not a full CP solver; just
    domain-specific propagators integrated into the move evaluation pipeline.

48. **CBLS invariants inform but don't replace resources.** The resource
    pattern (`init/merge/merge_reverse/excess`) is a specialized CBLS
    invariant system optimized for O(1) sequential evaluation via
    prefix/suffix caching. Generic CBLS (Hexaly, OscaR) propagates through
    an arbitrary DAG at O(depth) per move — strictly slower for sequential
    problems. Assignment engine uses CBLS-style incremental `delta_assign`
    evaluation since it lacks sequential structure; routing/scheduling keep
    the faster resource system.

49. **Typed models, shared infrastructure.** Users pick the model type
    (`RoutingModel`, `ScheduleModel`, `AssignmentModel`, `PackingModel`) making
    intent explicit. Engines share metaheuristic shells (ILS, tabu, LA),
    solution pool, cost evaluation, CLI, timing — but solution representations
    are fundamentally different per engine. No auto-detection of problem type.
    This is what every successful solver does (OR-Tools, Hexaly internally).

50. **Python bindings via nanobind.** C++ is the implementation language;
    Python (nanobind + scikit-build-core) is the primary user interface.
    All model types, Result, and solve() are bound 1:1. `pip install
    coso`. Python bindings added after routing engine works (step 6)
    to avoid binding churn during API evolution.

51. **Extract don't abstract.** Build routing first. When building scheduling,
    extract what's genuinely shared into `search/`. Don't pre-build abstract
    Engine/Move/Resource base classes for engines that don't exist yet.
    Three similar lines > premature abstraction.

52. **Resource dogfooding.** Built-in resources (LoadResource, DurationResource,
    DistanceResource) use the same `init/merge/merge_reverse/excess` interface
    as user-defined resources. If you want to reimplement capacity tracking,
    write a new resource with the same interface. Exception: if the generic
    interface introduces measurable overhead in a hot-path resource, a
    specialized fast path is allowed — but this is the exception. Performance
    trumps purity, but the generic interface must still work.

53. **Technician routing is rich VRP.** TRSP = VRP + skills + time windows +
    team formation + synchronization. Handled by the routing engine with
    SkillFilter, DurationResource, and a SyncResource for multi-technician
    visits. OR-Tools handles this through their RoutingModel dimensions;
    we handle it through resources. Same problem, same engine.

---

## 10. References

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
- SchedulingBenchmarks.org (Curtois). Nurse rostering (BCV, GPost, SINTEF, ORTEC,
  Ikegami, Montreal — 100+ instances), shift scheduling (24 instances), multi-activity
  multi-day scheduling (225 instances). XML format. https://www.schedulingbenchmarks.org
- ROADEF 2007 (France Telecom). Technician routing & scheduling with skills and teams.
  https://roadef.org/challenge/2007/

Technician routing & scheduling:
- Kovacs, Parragh, Doerner & Hartl (2012). *ALNS for service technician routing and
  scheduling*. J. Scheduling 15:579–600.
- Cordeau, Laporte, Pasin & Ropke (2010). *Scheduling technicians and tasks in a
  telecommunications company*. J. Scheduling 13:393–409.
- Hashimoto, Boland & Savelsbergh (2018). *Technician routing with stochastic service
  times*. Transportation Science.

Liner shipping:
- Koza, Desaulniers & Ropke (2020). *Integrated liner shipping network design and
  scheduling*. Transportation Science 54(2):512–533.
  https://doi.org/10.1287/trsc.2018.0888
- LINERLIB. Benchmark suite for liner shipping network design (7 base instances,
  21 variants). https://github.com/blof/LINERLIB

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
- Google OR-Tools / CP-SAT. RoutingModel dimensions, driver breaks, type
  incompatibilities, LIFO/FIFO PD, depot resources, GLS, MAB operator selection,
  global span, portfolio solving, solution finalization, automaton constraint,
  scheduling LNS neighborhoods (time-window/precedence/resource relaxation),
  conflict-directed search, adaptive LNS difficulty.
  https://github.com/google/or-tools
- VROOM. Production C++ VRP: skills matching, location-aware setup times,
  multiple time windows, max tasks, vehicle type-specific service, speed factor,
  three-matrix cost model (cost/duration/distance), per-task-hour cost,
  RouteSplit operator. https://github.com/VROOM-Project/vroom
- JuLS (Amazon). CP-as-move-filter for CBLS: constraint propagation prunes
  infeasible candidate moves before local search scoring. DAG-based invariants
  with incremental delta evaluation. https://github.com/amazon-science/JuLS
- OscaR/CBLS. Academic CBLS framework with sequence variables for routing.
  Demonstrates that generic CBLS needs domain-specific sequence/list variables
  to compete on routing — confirming our resource-first approach.
  https://github.com/cetic/oscar-cbls

LLM/Neural:
- Ye et al. (2025). *VRPAgent: LLM-driven operator discovery*. arXiv.
- Liu et al. (2024). *Evolution of Heuristics (EoH)*. ICML.
- Romera-Paredes et al. (2024). *FunSearch*. Nature.
