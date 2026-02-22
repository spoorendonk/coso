# CP-SAT Scheduling Research for primal-rsp

Deep analysis of Google's CP-SAT solver (OR-Tools `ortools/sat/`) with focus
on scheduling, assignment, and packing features. Each feature assessed for
relevance to primal-rsp's heuristic approach.

---

## 1. Interval Variables

### What CP-SAT does

CP-SAT represents scheduling activities through `IntervalConstraintProto`
with three affine expressions: `start`, `end`, `size`. The modeling API
(`cp_model.h`) provides:

```cpp
// Fixed-size interval: end - start == size (constant)
IntervalVar NewFixedSizeIntervalVar(const LinearExpr& start, int64_t size);

// Flexible interval: end - start == size (variable)
IntervalVar NewIntervalVar(const LinearExpr& start, const LinearExpr& size,
                           const LinearExpr& end);

// Optional interval: exists only if presence literal is true
IntervalVar NewOptionalIntervalVar(const LinearExpr& start,
                                    const LinearExpr& size,
                                    const LinearExpr& end, BoolVar presence);

IntervalVar NewOptionalFixedSizeIntervalVar(const LinearExpr& start,
                                             int64_t size, BoolVar presence);
```

Key design insight from `intervals.h`: The `IntervalsRepository` does not
enforce `start + size == end` itself -- it delegates that to linear
constraints. The repository tracks:
- `AffineExpression` for start, end, size (not raw variables -- affine
  transformations `a*var + b`)
- Presence literal for optional intervals
- Conditional bounds propagation: for optional intervals, bounds are
  propagated *as if* the interval is present

The `IntervalDefinition` struct (in `scheduling_helpers.h`) captures the
four components: `{start, end, size, optional<presence>}`.

### Assessment for primal-rsp

**(a) Already planned.** primal-rsp's scheduling model uses operations with
`machine` + `duration`, which implicitly creates fixed-size intervals.
The `ScheduleModel::add_operation()` API maps to this.

**(b) Relevant -- should add:**
- **Variable-size intervals.** Important for MRCPSP where different modes
  have different durations. We need a representation that tracks min/max
  duration when mode is not yet selected.
- **Optional intervals with presence literal.** Critical for FJSP
  alternatives and multi-mode RCPSP. CP-SAT's pattern: create one optional
  interval per (task, machine) combination, constrain "exactly one present"
  per task. primal-rsp should adopt this representation internally even
  though heuristics don't do constraint propagation -- the move operators
  need to know which intervals are present (assigned to which machine).

**(c) Exact-only:** Affine expression representation (`a*var + b`) is an
optimization for presolve/propagation. Not relevant for heuristic local search.

---

## 2. No-Overlap Constraint (Disjunctive)

### What CP-SAT does

`NoOverlapConstraintProto` takes a list of interval indices and enforces
mutual exclusion in time. This is the **disjunctive constraint** in
scheduling terminology.

Source: `ortools/sat/` implements multiple propagation levels:
1. **Basic bound propagation** via `SchedulingConstraintHelper`
2. **Not-first / Not-last** reasoning
3. **Edge finding** (from `timetable_edgefinding.h`)
4. **Precedence-based propagation** (`use_precedences_in_disjunctive_constraint`)
   -- uses transitive closure of known precedences to tighten bounds
5. **Strong propagation** (`use_strong_propagation_in_disjunctive`) --
   creates one Boolean literal for each pair of tasks encoding their
   relative order (up to `max_size_to_create_precedence_literals_in_disjunctive`,
   default 60)
6. **Dynamic precedence branching** (`use_dynamic_precedence_in_disjunctive`)
   -- at search time, instead of fixing interval bounds, decides "A before B"
   or "B before A"

For the 2D variant (`NoOverlap2DConstraintProto`), CP-SAT creates pairs
of x-intervals and y-intervals for rectangles. Propagation includes:
- Timetabling projection (project 2D to 1D cumulative on each axis)
- Energetic reasoning (area-based)
- Pairwise reasoning for small instances
- Try-edge reasoning
- Boolean relations for pairs of boxes (up to `no_overlap_2d_boolean_relations_limit`)

### Assessment for primal-rsp

**(a) Already planned.** Disjunctive constraint is implicit in JSP/PFSP/OSSP
through machine-assignment. The disjunctive graph representation (conjunctive
arcs = job precedences, disjunctive arcs = machine sequencing) is standard
for scheduling local search.

**(b) Relevant -- should add:**
- **2D no-overlap.** Relevant for 2D bin packing variants (cutting stock,
  strip packing). primal-rsp's packing model should support rectangle items
  with 2D positioning, not just 1D bin packing.
- **Precedence decision heuristic.** CP-SAT's idea of branching on "A before
  B" rather than "start_A = 5" is relevant for scheduling local search. Our
  operators already work this way (swap, insert, relocate change relative
  ordering) but the *selection* strategy -- which pair to order next -- should
  consider the overlap-based ordering from CP-SAT's
  `DisjunctivePrecedenceSearchHeuristic`. It picks the pair with the earliest
  start_min among overlapping intervals.

**(c) Exact-only:** Edge finding, not-first/not-last, and transitive closure
propagation are domain reduction techniques for exact solving. Not applicable
to local search (we always have a complete assignment).

---

## 3. Cumulative Constraint

### What CP-SAT does

`CumulativeConstraintProto` has:
- `capacity`: linear expression for resource capacity
- `intervals[]`: list of interval indices
- `demands[]`: linear expressions for resource demands per interval

Propagation levels (all configurable via parameters):

1. **TimeTablingPerTask** (`timetable.h`): Builds a resource profile from
   mandatory parts (the time window `[start_max, end_min)` where the task
   *must* be executing). Sweeps left-to-right to detect if any task cannot
   start at its earliest time due to resource conflicts.

2. **Overload checking** (`cumulative_energy.h`): Energy-based reasoning.
   For any time window `[a, b)`, the total energy (sum of `demand * size`
   for tasks that must fit in `[a, b)`) cannot exceed `capacity * (b - a)`.
   Uses a Theta-Lambda tree for efficient computation.

3. **Conservative Scale / Dual Feasible Functions**: Applies super-additive
   functions to demands before energy checking, tightening the bound. E.g.,
   if capacity is 10, a demand of 6 effectively uses "all" the capacity
   (rounded up).

4. **Timetable Edge Finding** (`timetable_edgefinding.h`): Combines
   timetabling with energy reasoning. Considers the mandatory profile plus
   the energy of a task to determine if the task can start at its earliest.

5. **Disjunctive detection** (`use_disjunctive_constraint_in_cumulative`):
   If a subset of tasks in a cumulative constraint are pairwise disjunctive
   (any two exceed capacity), use the stronger disjunctive propagators on
   that subset.

### Assessment for primal-rsp

**(a) Already planned.** RCPSP is in scope, which requires cumulative
resources. The SGS (Serial/Parallel Generation Scheme) constructs schedules
respecting cumulative capacity.

**(b) Relevant -- should add:**
- **Variable capacity.** CP-SAT supports variable capacity (as a linear
  expression). Relevant for problems where resource availability varies
  over time (e.g., workers available in shifts). primal-rsp should support
  time-varying resource availability for PRCPSP.
- **Variable demands.** CP-SAT supports variable demands per task. Relevant
  for MRCPSP where different modes consume different resource amounts.
- **Energy-based feasibility check.** Even in local search, we can use
  energy reasoning as a fast infeasibility filter: if total energy exceeds
  capacity * horizon for any time window, no feasible schedule exists. Cheap
  to compute and useful for pruning during move evaluation.
- **Disjunctive detection.** For cumulative resources where some task pairs
  are implicitly disjunctive (demands sum > capacity), we can apply
  disjunctive-specific operators (swap ordering). This happens naturally
  when capacity is small relative to demands.

**(c) Exact-only:** Timetable edge finding, theta-lambda trees, and DFF
propagation are domain reduction techniques. Not directly applicable.
However, the *concepts* inform what good feasibility checks look like.

---

## 4. Reservoir Constraint

### What CP-SAT does

`ReservoirConstraintProto` maintains a level starting at 0 that must stay
within `[min_level, max_level]` at all times. Events are:
- `time_exprs[]`: when each event occurs
- `level_changes[]`: how much each event changes the level (can be affine expressions)
- `active_literals[]`: whether each event is active

The level at time t = sum of `level_changes[i]` for all active events i
with `time[i] <= t`.

Two expansion modes:
1. **Precedence encoding** (default): Creates Boolean for each pair of
   events encoding their relative order. Then linear constraints enforce
   level bounds.
2. **Circuit encoding** (`expand_reservoir_using_circuit`): Uses a circuit
   constraint to find a permutation of same-time events.

### Assessment for primal-rsp

**(a) Already planned.** The plan mentions `reservoir` resource with events
in the scheduling recognition table.

**(b) Relevant -- should add:**
- **Level tracking as a resource type.** Reservoir semantics differ from
  cumulative: events happen at points in time (not over intervals), and the
  level persists between events. This is a distinct Resource type that needs
  its own `merge()` logic. The state tracks `{current_level, min_level_seen,
  max_level_seen}` along a sequence.
- **Produce/consume asymmetry.** Some events increase level, some decrease.
  The ordering of events matters for feasibility. This creates sequencing
  sub-problems similar to disjunctive scheduling but with a different
  feasibility criterion.

**(c) Exact-only:** The precedence and circuit encodings are linearization
strategies for exact solving. Local search just evaluates a sequence.

---

## 5. Sequence / Circuit Constraints

### What CP-SAT does

Two constraint types:

**CircuitConstraintProto**: Given a directed graph with arc presence
controlled by Boolean literals, find a Hamiltonian circuit. Every non-skipped
node has exactly one incoming and one outgoing arc. Self-loops allowed (to
skip a node).

**RoutesConstraintProto** ("Multiple Circuit" / VRP): Multiple circuits
through node 0 (depot). Each non-depot node has exactly one incoming and
one outgoing arc. Node 0 has `#incoming == #outgoing`. Supports
`dimensions` for cutting plane generation (e.g., load, time variables
per node).

Implementation (`circuit.h`): Propagation includes:
- Degree constraints (in/out degree = 1)
- No-cycle detection and enforcement
- Cut generation for LP relaxation using dimension information

### Assessment for primal-rsp

**(a) Already planned.** Routing is the primary domain for primal-rsp.
The route representation (sequence of clients per vehicle) implicitly
encodes this constraint.

**(b) Relevant -- should add:**
- **Circuit constraint for single-machine sequencing with sequence-dependent
  setup times.** The TSP-like structure of single-machine scheduling with
  setup times maps naturally to a circuit. This means operators from routing
  (2-opt, or-opt, relocate) can be reused for scheduling sequencing
  sub-problems.
- **Routing dimensions as a modeling concept.** CP-SAT's RoutesConstraint
  supports `NodeExpressions` for dimensions (load, time). This is similar to
  primal-rsp's Resource concept. Worth validating our resource design against
  this pattern.

**(c) Exact-only:** Cut generation from dimension information is LP-based.
The no-cycle propagator is tree search infrastructure.

---

## 6. Scheduling-Specific Search Strategies

### What CP-SAT does

`SchedulingSearchHeuristic` (in `integer_search.cc`, line 483):
- Scans all unfixed intervals
- Selects the one with smallest `(start_min, start_max, size_min)` with
  randomized tie-breaking
- Fixes presence (if optional), then start = start_min, then end = end_min
- Uses `next_decision_override` to fix all parts of the selected interval
  in sequence before returning to the main heuristic

This is a **"schedule left"** strategy: pack intervals as early as possible,
always fixing the interval that can start soonest.

`DisjunctivePrecedenceSearchHeuristic` (line 706):
- For each disjunctive constraint, finds the first pair of overlapping
  intervals (by start_min)
- Creates a precedence literal "A before B" and branches on it
- Selects the disjunctive with the smallest start_min among all candidates

`CumulativePrecedenceSearchHeuristic` (line 783):
- At the earliest time point where cumulative demand exceeds capacity, finds
  two tasks that can be made non-overlapping and creates a precedence decision

**Multi-worker strategy** (in `cp_model_search.cc`):
CP-SAT runs multiple search workers in parallel, each with different parameter
settings. For scheduling problems, workers include:
- `fixed` search (user-specified or scheduling heuristic)
- `auto` search
- `reduced_costs` with LP
- `lb_tree_search` with extra scheduling propagators
- `probing` with shaving

`AddExtraSchedulingPropagators` enables: overload checker, timetable edge
finding, DFF, strong disjunctive, hard precedences in cumulative, pairwise
reasoning for 2D.

### Assessment for primal-rsp

**(a) Already planned.** primal-rsp uses ILS/HGS which have their own
neighborhood exploration strategies.

**(b) Relevant -- should add:**
- **"Schedule left" as construction heuristic.** For initial solution
  construction in scheduling, this is essentially the SGS (serial generation
  scheme) for RCPSP or the semi-active schedule builder for JSP. primal-rsp
  should implement a "schedule-left" construction heuristic that mirrors
  this: iterate intervals by start_min, assign earliest feasible start.
- **Precedence-based move selection.** For local search, the idea of
  identifying overlapping interval pairs and deciding their order is
  directly applicable. Instead of randomly selecting a move, identify the
  earliest conflict point (where two tasks overlap on a machine or where
  cumulative capacity is exceeded) and focus moves there. This is
  "conflict-directed" local search.
- **Multi-strategy with different parameters.** Running multiple solver
  instances with different configurations is a portfolio approach. primal-rsp
  should support this at the algorithm level: run ILS with different
  perturbation strengths, tabu with different tenure, etc. in parallel.

**(c) Exact-only:** The tree search branching and backtracking mechanism
itself. The LP-based reduced cost strategy.

---

## 7. Objective Functions for Scheduling

### What CP-SAT does

CP-SAT supports arbitrary linear objectives. For scheduling, common patterns
in the examples:

**Makespan** (from `jobshop_sat.cc`):
```cpp
IntVar makespan = cp_model.NewIntVar(Domain(0, horizon));
cp_model.AddMaxEquality(makespan, {all job end times});
cp_model.Minimize(makespan);
```

**Weighted tardiness** (from `weighted_tardiness_sat.cc`):
```cpp
// tardiness[i] = max(0, completion[i] - due_date[i])
cp_model.AddGreaterOrEqual(tardiness[i], start[i] + duration[i] - due_date[i]);
objective += weight[i] * tardiness[i];
cp_model.Minimize(objective);
```

Also supports: total completion time, weighted completion time, total flow
time, number of tardy jobs (via Boolean indicators), lateness (can be
negative), earliness-tardiness, and multi-objective via lexicographic or
weighted sum.

The `AddMaxEquality` constraint is key for makespan: `target = max(exprs)`.
Internally decomposed into `target >= expr[i]` for all i, plus `target <=
max(expr[i])` via a lin_max constraint.

### Assessment for primal-rsp

**(a) Already planned.** The plan mentions `minimize_makespan()`,
`due_date` on job for tardiness. These are standard.

**(b) Relevant -- should add:**
- **Earliness-tardiness.** Some scheduling problems penalize both early
  and late completion (just-in-time scheduling). Should support
  `earliness_penalty` on jobs.
- **Multi-criteria scheduling.** Weighted sum of makespan + total weighted
  tardiness + total flow time. Support a structured objective that combines
  multiple scheduling criteria, not just a single one.
- **Job-level cost functions.** CP-SAT's `cost` field per task alternative
  allows machine-dependent processing costs. When a task runs on machine A
  it costs 10, on machine B it costs 15. Already relevant for FJSP.

**(c) Exact-only:** The `AddMaxEquality` encoding into linear constraints.
For heuristics, makespan is just `max(end_times)`.

---

## 8. Multi-Machine Scheduling (FJSP, Parallel Machines)

### What CP-SAT does

From `jobshop_sat.cc`, the FJSP pattern is:

```cpp
// For each task with alternatives:
for (int a = 0; a < num_alternatives; ++a) {
    BoolVar alt_presence = cp_model.NewBoolVar();
    IntervalVar alt_interval = cp_model.NewOptionalFixedSizeIntervalVar(
        alt_start, alt_duration, alt_presence);
    alternatives.push_back({machine[a], alt_interval, alt_presence});
}
// Exactly one alternative is present
cp_model.AddExactlyOne(interval_presences);

// Link alternative to main task
cp_model.AddEquality(main_start, alt_start).OnlyEnforceIf(alt_presence);
```

Each machine gets a `NoOverlap` constraint containing all (optional)
intervals assigned to it.

For parallel machines with identical machines, the pattern is the same
but all machines are symmetric -- which CP-SAT can detect and break
via symmetry.

**Cumulative relaxation** (`use_cumulative_relaxation` flag): When
multiple machines are alternatives for the same tasks, CP-SAT can add
a cumulative constraint over all grouped machines as a *relaxation*
(capacity = number of machines, demand = 1 per task). This tightens
bounds without full disjunctive reasoning on each machine.

### Assessment for primal-rsp

**(a) Already planned.** FJSP and parallel machines are explicitly in scope.

**(b) Relevant -- should add:**
- **Cumulative relaxation for machine groups.** For FJSP, adding a
  cumulative resource over all machines in a group (capacity = number
  of machines) provides a fast feasibility check during move evaluation.
  "Can this set of tasks possibly fit on these machines?" without checking
  individual machine sequences.
- **Alternative structure in data model.** primal-rsp should explicitly
  model the "task has alternatives" concept. A task has a list of
  (machine, duration) options. The current assignment selects one. Move
  operators include: reassign task to different machine, swap tasks
  between machines, relocate task with simultaneous machine change.
- **Symmetric machine handling.** For identical parallel machines,
  symmetry breaking reduces the search space. In local search terms:
  don't evaluate moves that just swap equivalent assignments between
  identical machines. primal-rsp should detect identical machines and
  skip redundant moves.

**(c) Exact-only:** The optional interval / exactly-one encoding is the
CP modeling pattern. For heuristics, we just track which machine each
task is assigned to.

---

## 9. Precedence Constraints

### What CP-SAT does

Precedences are modeled as simple linear constraints:
```cpp
cp_model.AddLessOrEqual(task_end, next_task_start);
// With lag time:
cp_model.AddLessOrEqual(task_end + lag, next_task_start);
// With negative lag (overlap allowed):
cp_model.AddLessOrEqual(task_end - overlap, next_task_start);
```

From `jobshop_sat.cc`:
```cpp
cp_model.AddLessOrEqual(end + precedence.min_delay(), start);
```

CP-SAT also supports:
- **Conditional precedences** via enforcement literals: precedence only
  applies if a Boolean is true
- **Transitive precedence closure** computed at root level
  (`transitive_precedences_work_limit` parameter)
- **Hard precedences in cumulative** (`use_hard_precedences_in_cumulative`):
  detects variables that must come after a set of cumulative intervals

### Assessment for primal-rsp

**(a) Already planned.** JSP has intra-job precedences (operation order).
RCPSP has general precedence graphs. The plan mentions "Activities +
precedences + resource reqs" for RCPSP.

**(b) Relevant -- should add:**
- **Lag times (min/max delay).** Support `min_lag` and `max_lag` between
  activities. `min_lag` = minimum time between end of A and start of B.
  `max_lag` = maximum time allowed. This is standard for RCPSP/max.
  Negative min_lag allows overlap (start-to-start precedences).
- **Conditional precedences.** For problems where precedence depends on
  assignment: "if task A and task B are on the same machine, A must
  precede B." This is implicit in disjunctive constraints but can also
  appear as problem-specific rules.
- **Generalized Temporal Constraints (GTC).** Beyond simple
  finish-to-start: start-to-start, finish-to-finish, start-to-finish
  with arbitrary lags. Standard in project scheduling.

**(c) Exact-only:** Transitive closure computation for domain propagation.

---

## 10. Alternative Constraints (Multi-Mode)

### What CP-SAT does

The "exactly one of these intervals is present" pattern:
```cpp
cp_model.AddExactlyOne(presence_booleans);
```

This is used for:
- FJSP: one machine per task
- MRCPSP: one mode per activity (different duration, different resource
  consumption per mode)

CP-SAT also supports `AddAtMostOne` (task may be skipped entirely) and
`AddBoolOr` / `AddBoolAnd` for more complex selection logic.

For multi-mode with shared resources, the pattern links mode selection
to resource consumption:
```cpp
cp_model.AddEquality(demand_var, mode_demand).OnlyEnforceIf(mode_presence);
```

### Assessment for primal-rsp

**(a) Already planned.** Multi-mode RCPSP (MRCPSP) is in scope.

**(b) Relevant -- should add:**
- **Mode selection as first-class concept.** In local search, "change mode"
  is a distinct move type from "change position in sequence." primal-rsp
  should have a `ChangeMode` operator that selects a different mode for
  an activity, potentially changing its duration and resource demands.
  This changes the evaluation but not the sequence.
- **Coupled mode-machine selection.** In FJSP, changing machine changes
  duration. In MRCPSP, changing mode changes both duration and resource
  requirements. The move operator must handle both simultaneously.

**(c) Exact-only:** The Boolean encoding of "exactly one present" is
for constraint propagation. Local search just maintains the current mode.

---

## 11. What CP-SAT Does That Pure Local Search Cannot

### Where CP-SAT shines:

1. **Optimality proofs.** CP-SAT can prove no better solution exists.
   Local search cannot. For small instances (say <20 jobs), CP-SAT often
   finds and proves optimal solutions faster than local search finds them.

2. **Lower bounds.** CP-SAT computes lower bounds via LP relaxation,
   energy reasoning, and constraint propagation. These bounds tell you
   how good your solution is. primal-rsp should *not* try to compute
   lower bounds -- that is a fundamentally different capability.

3. **Infeasibility detection.** CP-SAT can prove a problem has no feasible
   solution. Local search can only say "I couldn't find one."

4. **Complex logical constraints.** Constraints like "if A is on machine 1
   and B is on machine 2, then C must precede D" are naturally expressed in
   CP-SAT via enforcement literals. In local search, these become penalty
   terms or filters.

5. **Tight bounds on small subproblems.** Even when the full problem is too
   large for exact solving, CP-SAT excels at solving subproblems exactly.
   This is exactly what LNS exploits.

### Where local search shines:

1. **Large instances.** 1000+ jobs, 50+ machines. CP-SAT struggles here;
   local search scales linearly.

2. **Rich constraint combinations.** When a problem has 10+ constraint types
   simultaneously, the interaction between CP-SAT propagators becomes complex
   and can slow down. Local search evaluates the penalty sum in O(1).

3. **Anytime behavior.** Local search typically finds good solutions quickly
   and improves steadily. CP-SAT may spend a long time on the lower bound
   before finding good solutions (though its LNS workers help).

4. **Problem-specific operators.** Routing operators (2-opt, or-opt, SWAP*)
   and scheduling operators (critical path moves, block moves) exploit
   problem structure in ways that CP-SAT's generic branching cannot.

### Assessment for primal-rsp

**(b) Relevant -- the hybrid approach:**
- **LNS with CP sub-solver.** primal-rsp should support calling CP-SAT
  (or any MIP/CP solver) to solve subproblems generated by LNS. Fix most
  of the solution, relax a neighborhood, solve the subproblem exactly.
  CP-SAT's own LNS does this internally -- we should do it at the
  primal-rsp level too.
- **Initial solution from heuristics, polished by CP.** Provide the local
  search solution as a hint to CP-SAT for final polishing on small instances.
- **Lower bound information.** If available from an external solver, use
  the gap to decide when to stop local search.

---

## 12. CP-SAT for Assignment (Nurse Rostering, Timetabling)

### What CP-SAT does

**Core modeling pattern** (from employee scheduling docs):
```cpp
// Binary decision variable: shifts[(n, d, s)] = 1 if nurse n works shift s on day d
BoolVar shifts[(n, d, s)] = cp_model.NewBoolVar();

// Coverage: each shift-day has exactly one nurse
cp_model.AddExactlyOne({shifts[(0,d,s)], shifts[(1,d,s)], ...});

// Capacity: each nurse works at most one shift per day
cp_model.AddAtMostOne({shifts[(n,d,0)], shifts[(n,d,1)], ...});

// Fairness: min_shifts <= sum(shifts per nurse) <= max_shifts
// Preferences: maximize sum(request[n][d][s] * shifts[(n,d,s)])
```

**Table constraint** (`TableConstraintProto`): Extensionally defined
constraint -- list all allowed (or forbidden) tuples. Useful for shift
pattern rules: "these combinations of shifts over 3 consecutive days
are allowed."

**Automaton constraint** (`AutomatonConstraintProto`): A finite-state
automaton over a sequence of expressions. Transitions define
(state, label) -> next_state. The sequence must end in a final state.
Useful for:
- Shift pattern constraints: "no more than 3 consecutive night shifts"
  modeled as a DFA
- Work-rest patterns: FSM tracking consecutive work days
- Forbidden subsequences

**Inverse constraint** (`InverseConstraintProto`): `f_direct[i] == j <=>
f_inverse[j] == i`. Useful for bijection constraints in assignment.

**AllDifferent constraint**: All expressions take different values.
Useful for timetabling (no two lectures in same timeslot-room).

### Assessment for primal-rsp

**(a) Already planned.** Nurse rostering, employee scheduling, school
timetabling are in scope. The plan mentions automaton constraint for
forbidden sequences.

**(b) Relevant -- should add:**
- **Table constraint as a feasibility check.** For shift pattern rules
  defined extensionally (list of allowed 3-day patterns), store the allowed
  tuples in a hash set and check during move evaluation. O(1) lookup.
  This is more efficient than checking each rule individually.
- **Automaton constraint for pattern evaluation.** For complex shift
  pattern rules (max consecutive nights, min rest between shifts, weekend
  rules), encode as a DFA and evaluate by running the automaton over the
  assignment sequence. The violation is "number of states that reach a
  non-accepting state" or "distance to nearest accepting path." This is
  standard in nurse rostering literature (Bilgin et al.).
  primal-rsp already plans `forbidden_sequence(N, N, ...)` mapped to
  "Automaton constraint (FSM)" -- this is exactly CP-SAT's automaton.
- **AllDifferent for timetabling.** When assigning lectures to timeslots,
  "no two lectures in the same room at the same time" is an all-different
  constraint on (room, timeslot) pairs. In local search, violation = number
  of conflicts (pairs with same value).
- **Inverse constraint.** For assignment problems where we need both
  "which shift is nurse n assigned to on day d" and "which nurse is
  assigned to shift s on day d" -- maintaining both views enables efficient
  move evaluation. primal-rsp should maintain dual index structures.

**(c) Exact-only:** Value-based propagation for all-different (matching-based
filtering). The automaton expansion into a flow network.

---

## 13. Symmetry Breaking

### What CP-SAT does

Parameters (`symmetry_level`, default 2):
- Level 1: Detect symmetries in presolve, fix Booleans
- Level 2: Dynamic symmetry breaking during search
- Level 3: Detect symmetries for large models
- Level 4: Break maximum symmetry in presolve

`use_symmetry_in_lp`: Fold variables from the same orbit into a single
variable for LP relaxation.

`keep_symmetry_in_presolve`: Preserve symmetry group through all presolve
operations.

For scheduling, symmetry arises from:
- Identical machines in parallel machine scheduling
- Identical jobs (same processing times, same constraints)
- Identical workers in nurse rostering

### Assessment for primal-rsp

**(a) Already planned (partially).** No explicit symmetry breaking mentioned
in plan.

**(b) Relevant -- should add:**
- **Identical machine detection.** For parallel machines, detect identical
  machines and break symmetry in local search by: (a) canonical ordering
  of jobs on identical machines (e.g., first job on machine 0 has smallest
  index), (b) skip moves that only swap equivalent assignments between
  identical machines.
- **Identical worker detection.** For nurse rostering with identical skill
  sets, avoid evaluating swaps between equivalent workers.
- **Canonical form for solution comparison.** In population-based methods
  (HGS), distance between solutions should be invariant under symmetry.
  Otherwise the population wastes diversity on symmetry-equivalent solutions.

**(c) Exact-only:** Orbit-based variable folding for LP.

---

## 14. LNS in CP-SAT

### What CP-SAT does

CP-SAT has a rich LNS framework (`cp_model_lns.h`) with these generators:

**Generic neighborhood generators:**
- `RelaxRandomVariablesGenerator`: Random variable selection
- `RelaxRandomConstraintsGenerator`: Random constraint selection, relax all their variables
- `VariableGraphNeighborhoodGenerator`: BFS in variable-constraint graph
- `ArcGraphNeighborhoodGenerator`: Extend working set by one connected variable
- `ConstraintGraphNeighborhoodGenerator`: Connected constraint selection
- `DecompositionGraphNeighborhoodGenerator`: Tree-decomposition inspired
- `LocalBranchingLpBasedNeighborhoodGenerator`: LP-guided neighborhood (based
  on Huang et al. 2023)

**Scheduling-specific generators:**
- `RandomIntervalSchedulingNeighborhoodGenerator`: Relax random intervals
- `RandomPrecedenceSchedulingNeighborhoodGenerator`: Keep random precedences,
  relax the rest (fixes relative ordering, frees timing)
- `SchedulingTimeWindowNeighborhoodGenerator`: Relax intervals in a random
  time window (fix early tasks, fix late tasks, free middle)
- `SchedulingResourceWindowsNeighborhoodGenerator`: Per-resource time window
  relaxation

**Packing-specific generators:**
- `RandomRectanglesPackingNeighborhoodGenerator`: Random rectangle selection
- `RectanglesPackingRelaxOneNeighborhoodGenerator`: Relax one rectangle and
  its neighbors
- `RectanglesPackingRelaxTwoNeighborhoodsGenerator`: Relax two rectangles
  and neighbors (for swap-like improvements)
- `RandomPrecedencesPackingNeighborhoodGenerator`: Precedence-based
- `SlicePackingNeighborhoodGenerator`: Relax a dimensional slice

**Routing-specific generators:**
- `RoutingRandomNeighborhoodGenerator`: Random arc relaxation
- `RoutingPathNeighborhoodGenerator`: Relax consecutive arc sequences
- `RoutingFullPathNeighborhoodGenerator`: Relax entire path plus endpoints
  of other paths

**RINS generator:**
- `RelaxationInducedNeighborhoodGenerator`: Fix variables where incumbent
  and LP relaxation agree. Based on Danna et al. 2004.

**Key infrastructure:**
- Adaptive difficulty: Each generator tracks its success rate and adjusts
  the fraction of variables to relax (`difficulty` parameter).
  Uses `AdaptiveParameterValue` utility.
- Solution hinting: Neighborhoods include hints from the current solution.
- Parallel execution: Multiple generators run concurrently via the
  SubSolver framework.

### Assessment for primal-rsp

**(a) Already planned.** Ruin-and-recreate is planned for routing. The
ILS framework includes perturbation operators (random removal, worst
removal, related removal).

**(b) Relevant -- should add:**
- **Time-window-based LNS for scheduling.** Fix tasks before time T and
  after time T+W, free tasks in [T, T+W]. This is a natural scheduling
  neighborhood that exploits temporal locality. Should be a standard
  perturbation operator for scheduling problems.
- **Precedence-based LNS for scheduling.** Keep most precedence decisions
  (relative ordering), free a random subset. This is more structured than
  random removal -- it preserves the overall schedule shape while allowing
  local resequencing.
- **Resource-based LNS for scheduling.** For each resource, independently
  relax a time window. Different resources get different windows. This
  captures the locality of resource conflicts.
- **RINS-like neighborhood.** If primal-rsp maintains multiple solutions
  (population in HGS), fix variables where the best two solutions agree,
  free the rest. This is a structured crossover / LNS hybrid.
- **Adaptive difficulty.** Track acceptance rate per operator and adjust
  the perturbation size. If an operator generates accepted solutions 50%
  of the time, increase the perturbation (harder neighborhoods). If 5%,
  decrease (easier neighborhoods). CP-SAT's adaptive parameter value
  mechanism is a good reference.
- **LP-based / relaxation-guided neighborhoods.** If a continuous
  relaxation is available (from an external solver), use it to guide which
  variables to relax. This bridges exact and heuristic methods.

**(c) Exact-only:** The tree-decomposition neighborhood generator is
designed for CP-SAT's exact sub-solver. The idea of decomposition-aware
neighborhoods could inspire heuristic design, but the implementation
requires exact solving of the sub-problem.

---

## 15. Additional Constraint Types

### What CP-SAT has that we should consider:

**Element constraint** (`ElementConstraintProto`):
`expressions[index] == target`. Array indexing as a constraint. Useful
for: "the processing time of task i depends on which machine it's
assigned to" -- `duration[i] = processing_time_matrix[i][machine[i]]`.

**IntProd, IntDiv, IntMod constraints**: Arithmetic on integer variables.
Limited use in scheduling but relevant for cost calculations.

**LinMax / LinMin**: `target = max(exprs)` or `target = min(exprs)`.
Used for makespan modeling and for max/min over sets of expressions.

**BoolOr, BoolAnd, BoolXor, AtMostOne, ExactlyOne**: Boolean logic
constraints. Used extensively for mode selection, shift assignment,
and logical conditions.

**AddAbsEquality**: `target = |expr|`. Used for absolute deviation
objectives.

**AddMultiplicationEquality**: `target = expr1 * expr2`. For quadratic
terms in objectives.

**Enforcement literals on any constraint**: Half-reification.
`enforcement_literal => constraint`. This is pervasive in CP-SAT --
almost any constraint can be conditional. Used for:
- Optional tasks
- Mode-dependent constraints
- Conditional precedences
- Implications

### Assessment for primal-rsp

**(b) Relevant -- should add:**
- **Element constraint pattern for machine-dependent durations.** In FJSP,
  the duration of a task depends on which machine it's assigned to. Our
  internal representation should support this efficiently: a lookup table
  `duration[task][machine]` that is evaluated during move evaluation.
- **Conditional constraints via filters/penalties.** CP-SAT's enforcement
  literals correspond to our move filters and conditional penalties. Ensure
  primal-rsp supports conditional constraints cleanly: "this constraint
  only applies when condition X holds."
- **AbsEquality for earliness-tardiness.** `|completion - due_date|`
  penalizes both early and late. Support absolute-value terms in the
  penalty function.

---

## 16. CP-SAT's Feasibility Jump (Local Search within CP-SAT)

### What CP-SAT does

CP-SAT includes a local search component called **Feasibility Jump**
(`feasibility_jump.h/.cc`) that operates on the flat variable
representation (not on intervals directly).

Key components:
- **LsEvaluator** (in `constraint_violation.h`): Computes constraint
  violations for a complete assignment. Supports incremental updates
  via `WeightedViolationDelta`.
- **JumpTable**: For each variable, caches the best single-variable
  change (delta) and its score (weighted violation reduction).
- **Compound moves**: Sequences of single-variable changes evaluated
  together before committing.
- **Weight updates**: Constraint weights increase when violated
  (similar to GLS/breakout).
- **Slope breakpoints**: For each variable, the violation function is
  piecewise linear. The optimal jump is at a breakpoint.

For scheduling constraints specifically:
- `CompiledNoOverlapWithTwoIntervals`: Violation for pairs of intervals
- `CompiledNoOverlap2dConstraint`: 2D overlap violation
- `CompiledReservoirConstraint`: Reservoir level violation

The FJ approach works on the *expanded* model where intervals are
represented by their start/end/size variables. It does not use
scheduling-specific operators -- it makes single-variable moves.

### Assessment for primal-rsp

**(b) Relevant -- key insight:**
- CP-SAT's Feasibility Jump is essentially the same idea as our
  Feasibility Jump in `mip-heuristics` (the generic MIP local search).
  It works on the flat variable representation with weighted violations.
  **This validates our architecture split**: generic FJ for `mip-heuristics`
  (operates on `Ax <= b`), problem-specific local search for `primal-rsp`
  (operates on routes, sequences, schedules).
- CP-SAT's FJ is *weaker* than problem-specific local search for
  scheduling because it makes single-variable moves. Moving `start_A`
  by +1 is much less effective than "swap A and B on machine 3" which
  simultaneously changes multiple variables. **This is exactly why
  primal-rsp exists.**
- **Violation computation patterns.** The `CompiledNoOverlapWithTwoIntervals`
  violation (sum of overlaps between interval pairs) is a useful penalty
  design for scheduling local search. Our penalty for disjunctive
  violations should be similar: sum of max(0, end_A - start_B) for
  overlapping pairs on each machine.

---

## Summary Table

| # | Feature | Status | Action |
|---|---------|--------|--------|
| 1 | Interval variables (fixed, flexible, optional) | (a) Planned | Add variable-size and optional interval support |
| 2a | No-overlap 1D (disjunctive) | (a) Planned | Already in disjunctive graph |
| 2b | No-overlap 2D | (b) Relevant | Add for 2D packing problems |
| 3 | Cumulative constraint | (a) Planned | Add variable capacity/demands, energy feasibility check |
| 4 | Reservoir constraint | (a) Planned | Implement as distinct Resource type |
| 5 | Circuit/routes constraint | (a) Planned | Reuse routing operators for sequencing sub-problems |
| 6 | Schedule-left heuristic | (b) Relevant | Implement as construction heuristic |
| 6b | Precedence-based move selection | (b) Relevant | Conflict-directed operator selection |
| 6c | Multi-strategy parallel search | (b) Relevant | Portfolio of algorithm configurations |
| 7 | Multiple scheduling objectives | (a) Planned | Add earliness-tardiness, multi-criteria |
| 7b | Machine-dependent costs | (b) Relevant | Support per-machine processing costs |
| 8 | FJSP alternative encoding | (a) Planned | Explicit alternative data structure |
| 8b | Cumulative relaxation for machine groups | (b) Relevant | Fast feasibility check |
| 8c | Symmetric machine detection | (b) Relevant | Skip redundant moves |
| 9 | Precedence with lag times | (b) Relevant | Support min_lag, max_lag, GTC |
| 10 | Mode selection operators | (b) Relevant | ChangeMode as distinct move type |
| 11 | CP as LNS sub-solver | (b) Relevant | Support external exact solver for neighborhoods |
| 12a | Table constraint | (b) Relevant | Hash-set lookup for pattern rules |
| 12b | Automaton for pattern evaluation | (a) Planned | DFA-based shift pattern checking |
| 12c | AllDifferent for timetabling | (b) Relevant | Conflict-counting violation |
| 12d | Inverse / dual index structure | (b) Relevant | Maintain both assignment views |
| 13 | Symmetry breaking | (b) Relevant | Identical machine/worker detection |
| 14a | Time-window LNS | (b) Relevant | Standard scheduling perturbation |
| 14b | Precedence-based LNS | (b) Relevant | Order-preserving perturbation |
| 14c | Resource-based LNS | (b) Relevant | Per-resource time window relaxation |
| 14d | RINS-like neighborhood | (b) Relevant | Agreement-based crossover |
| 14e | Adaptive difficulty | (b) Relevant | Track acceptance rate per operator |
| 15a | Element constraint pattern | (b) Relevant | Machine-dependent duration lookup |
| 15b | Conditional constraints | (b) Relevant | Filters + conditional penalties |
| 15c | Abs for earliness-tardiness | (b) Relevant | Support absolute-value penalties |
| 16 | Feasibility Jump as LS | (c) Generic LS | Validates architecture split with mip-heuristics |

---

## Key Takeaways

1. **CP-SAT's strength is propagation; ours is problem-specific moves.**
   CP-SAT's interval/cumulative/disjunctive propagators are domain reduction
   techniques that tighten bounds during tree search. We don't need those.
   What we *do* need are the modeling concepts (intervals, optional tasks,
   modes, alternatives) and the violation computation patterns.

2. **CP-SAT's LNS framework is directly relevant.** The scheduling-specific
   neighborhood generators (time window, precedence-based, resource-based)
   are excellent perturbation operators for our ILS/LNS. These should be
   standard operators in primal-rsp's scheduling engine.

3. **The alternative/mode concept is critical.** FJSP and MRCPSP require
   simultaneous machine/mode selection and sequencing. CP-SAT models this
   via optional intervals + exactly-one. We should model it as explicit
   alternatives with dedicated operators (ChangeMode, ReassignMachine).

4. **Automaton constraints are important for assignment.** Nurse rostering
   requires complex pattern rules. CP-SAT's automaton constraint is the
   right abstraction. primal-rsp already plans this (FSM in recognition
   table) -- confirm it is first-class.

5. **Hybrid CP+LS is the state-of-the-art.** CP-SAT itself combines exact
   search, LP relaxation, LNS, and feasibility jump. primal-rsp should
   support calling an exact solver (CP-SAT, CPLEX, Gurobi) for LNS
   subproblems when available, while being self-contained (pure heuristic)
   when no external solver is available.

6. **Violation computation matters.** CP-SAT's `constraint_violation.h`
   shows how to efficiently compute and incrementally update violations
   for scheduling constraints. Our Resource/penalty system serves the same
   purpose but with problem-specific data structures (routes, sequences)
   rather than flat variables.
