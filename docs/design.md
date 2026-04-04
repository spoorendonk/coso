# COSO Design

Architecture, design decisions, and references.

## Internal Architecture

The user never sees this. The model translates their declaration into engine
components.

### Three-layer engine

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

### Resources: the extensibility mechanism

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

### Cost model

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

### Operators

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

### Search control features

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

### Construction heuristics

**Routing:** Nearest-neighbour (default), savings algorithm (Clarke-Wright).
Both seed routes, then local search improves.

**Assignment:** First Fit Decreasing — sort entities by difficulty (most-
constrained employee first: fewest available shifts, most skill requirements),
assign to best available value. Cheapest insertion — evaluate all entity-value
pairs, pick the one with lowest cost increase.

**Scheduling:** NEH (flow shop), priority-rule dispatch (job shop), serial/
parallel SGS (RCPSP).

### Assignment engine operators

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

### CP-as-move-filter for assignment/packing engines

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

### LLM-friendly operator interfaces

For LLM-driven heuristic discovery (VRPAgent, EoH, FunSearch):

```cpp
// Clean function signatures LLMs can target
using DestroyFn = std::function<std::vector<int>(Solution&, int count, RNG&)>;
using RepairFn  = std::function<void(Solution&, const std::vector<int>&,
                                      const ProblemData&)>;
using PerturbFn = std::function<Solution(const Solution&, const ProblemData&, RNG&)>;
```

## Relationship to mip-heuristics

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

## Infrastructure Roadmap

### Repo skeleton

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 1.1 | CMake + CI setup | Project compiles, `ctest` runs (0 tests) | CMakeLists.txt, cmake/, .github/ | — |
| 1.2 | Shared types header | `types.h` with Coord, TimeWindow, CostParams, Result, TimeLimit | src/model/types.h | Done |
| 1.3 | Model headers (declarations only) | All 4 model classes declared, compile with no impl | src/model/routing_model.h, schedule_model.h, assignment_model.h, packing_model.h | Done |
| 1.4 | API contract tests | Tests that exercise model API (compile + link, assert on trivial cases) | tests/model/model_test.cpp | Done |

1.1 and 1.2 are parallel. 1.3 needs 1.2. 1.4 needs 1.1 + 1.3.

### Python bindings

```
Deliverable: `pip install coso`, Python API mirrors C++.
Touches only python/ directory — can run parallel with step 5 resources.
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 6.1 | nanobind + scikit-build-core setup | `pip install -e .` builds extension module | python/bindings.cpp, pyproject.toml, CMakeLists.txt (extend) | 2.9 |
| 6.2 | RoutingModel Python bindings | `coso.RoutingModel` mirrors C++ API | python/bindings.cpp (routing section) | 2.9 |
| 6.3 | Python test suite | pytest tests for model API + solve + result access | python/tests/ | 6.2 |

### E2E matrix + harness

```
Deliverable: one executable E2E framework that can run scenario files and
validate correctness invariants across all model families.
This is mandatory before claiming "all roadmap variants covered".
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 11.1 | Variant coverage matrix | `docs/e2e-matrix.md`: every roadmap variant (R/N/S/A/K/P) mapped to one or more scenario IDs, status, and owning work unit | docs/e2e-matrix.md | 4.x catalog |
| 11.2 | Scenario schema | JSON schema for E2E scenarios (model type, inputs, expected checks, deterministic limits) | examples/e2e/schema.json | 11.1 |
| 11.3 | Unified E2E runner | `e2e_runner` builds model from scenario, solves, emits normalized JSON result | examples/e2e/e2e_runner.cpp | 11.2 |
| 11.4 | Invariant check library | Shared checks: feasibility, capacity, TW, precedence, flow conservation, assignment completeness, packing validity | examples/e2e/checks.* | 11.3 |
| 11.5 | CTest integration | `e2e-smoke` and `e2e-benchmark` labels + helper scripts | tests/e2e/, tests/CMakeLists.txt | 11.3 |
| 11.6 | Baseline smoke pack | One tiny deterministic scenario per model family currently public | examples/e2e/scenarios/smoke/*.json | 11.4, 11.5 |
| 11.7 | Deterministic perf gate tooling | Compare candidate vs baseline on `work_units` (median ratio gate) | tests/perf/e2e_check_regression.py | 11.5 |

### Public model API completion

```
Deliverable: all roadmap model families have public model APIs comparable to
Routing/Scheduling/Assignment/Packing, with typed Result access.
Hard truth: without this, "all models E2E" is impossible.
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 12.1 | NetworkModel public API | `src/model/network_model.{h,cpp}` + `Result::flows()` integration + C++ tests | src/model/, tests/model/, tests/network/ | 10.10 |
| 12.2 | LotSizingModel public API | `src/model/lotsizing_model.{h,cpp}` delegating to lotsizing engine / mip-heuristics bridge | src/model/, src/lotsizing/, tests/lotsizing/ | 10.9 |
| 12.3 | Python exposure for new models | Python bindings and pytest coverage for Network/LotSizing APIs | python/bindings.cpp, python/tests/ | 12.1, 12.2 |
| 12.4 | Model registry/docs alignment | README + roadmap + examples reflect complete public model surface | README.md, docs/roadmap.md | 12.1, 12.2 |

### Solve path completion

```
Deliverable: ScheduleModel, AssignmentModel, PackingModel solve methods produce
real solutions on baseline benchmark classes (not stubs).
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 13.1 | Schedule solve baseline | Real solve path for JSP/FJSP/RCPSP with objective + Result population | src/model/schedule_model.cpp, src/scheduling/ | 7.x |
| 13.2 | Assignment solve baseline | Real solve path for NRP/ESP/MATSP baseline constraints/operators | src/model/assignment_model.cpp, src/assignment/ | 8.x |
| 13.3 | Packing solve baseline | Real solve path for BPP/VBP/BPPC with move/swap/merge loop | src/model/packing_model.cpp, src/packing/ | 9.x |
| 13.4 | Cross-model deterministic stop parity | Ensure time/work stop semantics are consistent across all model solve paths | src/model/*, tests/model/ | 13.1-13.3 |

### Variant E2E scenario packs

```
Deliverable: scenario packs that correspond directly to roadmap variants.
Each work unit adds scenario files + expected checks + CTest registration.
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 14.1 | Routing core pack | CVRP, VRPTW, heterogeneous fleet, pickup-delivery, optional/groups, multi-trip, profiles | examples/e2e/scenarios/routing/ | 11.6, 13.1-13.3 |
| 14.2 | Routing extended pack | Skills, sync, breaks, compartments/loading, task-count, type incompatibility | examples/e2e/scenarios/routing_extended/ | 14.1 |
| 14.3 | Network pack | MCF, RCMCF, liner shipping small instances | examples/e2e/scenarios/network/ | 12.1 |
| 14.4 | Scheduling pack | JSP, FJSP, RCPSP, parallel machine baselines | examples/e2e/scenarios/scheduling/ | 13.1 |
| 14.5 | Assignment pack | NRP, employee scheduling, multi-activity scheduling baselines | examples/e2e/scenarios/assignment/ | 13.2 |
| 14.6 | Packing pack | BPP, VBP, BPPC baselines | examples/e2e/scenarios/packing/ | 13.3 |
| 14.7 | Production pack | CLSP, MLCLSP delegated flow | examples/e2e/scenarios/lotsizing/ | 12.2 |
| 14.8 | Format/parser pack | CVRPLIB, Solomon, Li-Lim, Taillard, PSPLIB, NRP parser-driven E2E cases | examples/e2e/scenarios/parsers/ | 14.1-14.7 |

### CI gates + regression policy

```
Deliverable: CI enforces e2e-smoke correctness and deterministic perf trend.
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 15.1 | Required smoke gate | `e2e-smoke` required in CI for PRs | .github/workflows/* | 11.5, 14.x baseline packs |
| 15.2 | Nightly benchmark gate | `e2e-benchmark` nightly run with artifact retention | .github/workflows/* | 11.7, 14.x |
| 15.3 | Deterministic perf thresholds | Per-pack `work_units` regression thresholds + fail policy | tests/perf/, docs/ | 15.2 |
| 15.4 | Flake quarantine process | Label/quarantine mechanism for unstable scenarios with owner + SLA | docs/testing.md, workflow scripts | 15.1 |

### Documentation + operator handoff protocol

```
Deliverable: agents can pick work units safely and produce consistent E2E PRs.
```

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 16.1 | E2E contribution guide | How to add scenarios, expected checks, deterministic limits, and labels | docs/e2e-contributing.md | 11.x |
| 16.2 | Variant ownership board | Per-variant owner + status dashboard linked from roadmap | docs/e2e-matrix.md (ownership fields) | 11.1 |
| 16.3 | PR template for E2E units | Required evidence: scenario IDs, checks, work-unit deltas, ctest labels | .github/pull_request_template.md | 15.1 |
| 16.4 | Canonical examples refresh | One polished end-to-end example per model family in README/examples | README.md, examples/ | 14.x |

### Parallelism and sequencing

```
Required sequence:
  11.x foundation → 12.x API completion + 13.x solve completion → 14.x packs
  → 15.x CI gates → 16.x docs/handoff hardening.

Hard truth:
  "All models + all roadmap variants E2E" cannot be delivered in one PR because
  some model APIs are missing and several solve paths are still stubs.
```

**Parallel-safe lanes:**
- `11.1` can run in parallel with `12.1/12.2`.
- `12.1` (network API) and `12.2` (lotsizing API) are parallel.
- `13.1`, `13.2`, `13.3` are parallel (different directories/teams).
- `14.3`, `14.4`, `14.5`, `14.6`, `14.7` are parallel once corresponding 12/13 deps are green.
- `15.2` can start before all `14.x` are complete (nightly on available packs).
- `16.x` can be incremental throughout but must be finalized after `15.1`.

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
Step 11: 11.1/11.2/11.5 parallelizable after schema + runner landing
Step 12: 12.1 and 12.2 parallel; 12.3 after both
Step 13: 13.1/13.2/13.3 parallel; 13.4 after all three
Step 14: model-family packs parallel by dependency lane
Step 15: 15.1 first, 15.2/15.3 parallel, 15.4 independent
Step 16: mostly parallel documentation hardening
```

### Agent coordination rule

Before starting work, check open branches and PRs.
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

## Key Design Decisions

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

## References

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
