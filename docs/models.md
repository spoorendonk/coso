# Model spec

What can be declared, and what each engine does with it. The counterpart is `docs/backends.md`
(#176) — what an engine must do with a declaration.

One tracked file, so a schema cut in `src/model/*.h` and its spec row land in the same commit and
the same review. `docs/` had been empty since #174 deleted five stale files; the guard against that
staleness is an evidence rule, not the absence of docs: every cell carries the evidence its value
requires (below), and a cell without it is a review finding.

The six per-model sections are filled by the audits (#200–#205), each in the template below.

## Preamble — five principles

1. **Structure, not formulation.** A model declares routes, schedules, rosters, bins, flows,
   lots. COSO exposes no set-covering, set-partitioning, or generic-MIP archetype — README's
   "no LP/MIP flattening in the user's hands", made binding. Anyone holding a covering matrix
   has HiGHS.
2. **Models do not compose.** A variant whose *solution* is two structures joined by an outer
   loop — inventory routing (R22, #125) and its relatives such as MIRPLIB — is a user-level
   loop over two models, not a seventh archetype and not a protocol feature. This says nothing
   about variants that are one model with a wider schema (location-routing, transshipment,
   GLSP, multi-machine lot sizing): those are `extend`/`cut` rulings on scope, made by the
   owning audit.
3. **A feature an engine does not enforce is not a feature of that engine.** Hence the
   two-axis vocabulary.
4. **Reference solutions are declarations; warm-start hints are protocol inputs.** Where
   anything in the model is defined against a prior solution — pinned clients, a change
   penalty against a published roster — that solution is part of the problem: declarable,
   introspectable, and an engine must honour or reject it. A prior solution with no semantic
   effect is a hint: it travels on the `solve()` call, not in the model, and an engine may
   ignore it. Today `RoutingModel::set_initial_routes()` plays both roles;
   `ScheduleModel::set_initial_schedule()` is a hint; `AssignmentModel::set_published_schedule()`
   + `set_change_penalty()` is a declaration. Each audit applies this to its model; #176
   introspects reference solutions and pins and carries hints on the call.
5. **Cuts land in E2; additions are additive.** #176 builds introspection on the schema
   *after* E2's cuts. Every `extend` ruling lands later, in its owning milestone, as an
   addition that leaves the existing schema and accessors valid. No E2 issue adds a setter, a
   field, or wiring.

## Vocabulary — two axes

**Model axis**, per feature: `declarable` (the API accepts it) | `absent` (it cannot be said).

**Engine axis**, one column per engine in #173's map for that model — routing: native, PyVRP;
scheduling and assignment: native, CP-SAT; packing and lot sizing: native, HiGHS; network:
native, OR-Tools min-cost flow, later mcfcg / HiGHS. Cell values and their evidence:

| cell | meaning | evidence |
|---|---|---|
| `supported` | the engine enforces the declaration | a test through the model API whose assertion checks the *returned solution* against the declaration — never `Result::feasible()` alone |
| `documented` | the engine's public API accepts the feature; no plugin exists yet to test it | the engine's API reference, version pinned. The only value an unintegrated engine may hold; its milestone turns it into `supported` or `rejects` |
| `rejects` | the engine refuses the model with a clear error | the throw, and a test that hits it |
| `drops` | the engine accepts the declaration and ignores or misreports it | the bug issue. Must be zero in an engine's column before that engine's milestone closes |
| `—` | the engine cannot express it | one line saying why |

A native `drops` / `rejects` cell carries an implementation note that decides wire-vs-delete:
**dormant** (engine code exists but is unreachable from `solve()` — name the idle file) or
**dead** (no code).

**Deletion rule — suspended.** As written, this rule deleted a `declarable` feature when no
engine column was `supported` or `documented`. It made the schema a function of engine
capability, which is the coupling the current phase exists to break: the model API is the
product, the engine is a reference implementation, and its gaps are evidence about the engine
and not a verdict on what a user may declare (CLAUDE.md, **Current phase**).

For now: a feature with no supporting engine **stays declarable**, and the engine columns
record what each engine does with it — `drops` included. Deleting from the schema needs a
*modelling* reason: the feature cannot be said coherently, it duplicates another, or it belongs
to a different archetype. The deletions already made under the old rule each carry such a
reason too, so they stand.

What is *not* suspended: a declaration that is silently ignored must say so in its row, with
its issue number. Accepted-and-dropped is tolerable and tracked; undocumented is not.

**Feature granularity.** One feature name per thing an engine in the model's map either has or
lacks, so an engine's reject list is a column difference. `time_windows` and
`multiple_time_windows` are two features; `capacity` with N dimensions is one.

## Section template

Every model section has six parts, in this order:

1. **Schema** — the declarable entities and fields after cuts.
2. **Features** — name · model axis · one column per engine · evidence. This is what plugins
   declare support against.
3. **Result** — what a returned solution must carry for a third party to verify every
   `supported` feature. `Result::cost` **is the value of the declared objective**; each
   objective is itself a feature row. (Routing returns total distance regardless of declared
   fixed or duration cost — #198.)
4. **Variants** — each claimed variant (README row, R/S/A/N/K/P issue) → the features it needs
   → verdict: expressible now / after a named `extend` / cut.
5. **Rulings** — absent classes and scope questions that land in this model; each `extend`
   with an additive API sketch and an owner, each `cut` with the reason.
6. **Defects** — the B-issues and any the audit files.

## Rulings on the classes #175's comment lists

| class | ruling | home |
|---|---|---|
| set covering / partitioning | `cut` — a formulation (principle 1) | preamble |
| crew pairing | `cut` — set partitioning over generated pairings (principle 1) | preamble |
| standalone facility location | `cut` as an archetype; reducible to fixed-charge network design (open facility = open arc from a super-source) once #184's design API exists. The network audit demonstrates the reduction | preamble, #184 |
| 2D / 3D packing | `cut` — geometry is a different algorithm family with no oracle in #173's engine map. The packing audit records the reason | preamble |
| knapsack | `cut` — HiGHS solves it exactly, nothing structure-aware is planned, and the objective is not bin minimisation. The packing audit records it | preamble |
| TSP and variants | expressible now: `RoutingModel`, one vehicle, no capacity (TSPTW and PC-TSP likewise). No archetype. Says nothing about engine competitiveness — that is #178's | routing audit adds the tests |
| cutting stock (1-D) | deferred to the packing audit: expected `extend` via item multiplicity, since BPPLIB carries CSP instances and #182's arc-flow formulation is the CSP formulation | packing audit |
| crew rostering | deferred to the assignment audit: it is what `AssignmentModel` already is, modulo "shift" vs "duty" — pending that audit's archetype ruling | assignment audit |

## Network

`NetworkModel` (`src/model/network_model.h`) declares a directed graph with node supplies and
arc costs and capacity bounds, and solves it as a single-commodity minimum-cost flow. The
resource API (`add_resource` / `set_resource_usage`) is deleted as of this section — see
§Rulings, #195.

Engine columns are #173's map for this model: **native** (`McfSolver`, successive shortest
paths), **OR-Tools** `SimpleMinCostFlow` (#179), **mcfcg** and **HiGHS** (#184). Only the native
engine is integrated; nothing else is a dependency of this repo, so the other three columns hold
`documented` where the engine's API has the feature and `—` where it does not, each pinned to a
public API reference:

- OR-Tools v9.11, `ortools/graph/min_cost_flow.h` (`SimpleMinCostFlow`).
- mcfcg (<https://github.com/spoorendonk/mcfcg>, arXiv:2509.24656) — column generation for
  min-cost multicommodity flow, path- and tree-based Dantzig-Wolfe. No tagged release, so the
  pin is a commit: README formulations at `ad848c25d8fd` (2026-09-03).
- HiGHS v1.7.2, `HighsLp` (`col_cost_`, `col_lower_`, `col_upper_`, `row_lower_`, `row_upper_`).

### Schema

| entity | fields |
|---|---|
| node | `supply` (int; positive = supply, negative = demand), `name` (label only) |
| arc | `tail`, `head`, `cost` (int, per unit of flow), `lower_cap`, `upper_cap` |

`add_arc` throws `std::out_of_range` on an unknown node and `std::invalid_argument` on
`lower_cap < 0` or `upper_cap < lower_cap`. `solve(TimeLimit)` is the whole call surface;
there is no reference solution and no warm start, so principle 4 has nothing to rule here.

Absent, and named because #184 needs them: commodities, arc-open decisions and fixed costs,
capacity modules, per-commodity cost or admissibility, path-count and hop limits, undirected
links, and node balance as an inequality rather than an equality.

### Features

| feature | model | native | OR-Tools `SimpleMinCostFlow` | mcfcg | HiGHS |
|---|---|---|---|---|---|
| node supply | `declarable` | `supported` [a] | `documented` [e] | `documented` [i] | `documented` [m] |
| arc cost | `declarable` | `supported` [b] | `documented` [f] | `documented` [j] | `documented` [n] |
| `upper_cap` | `declarable` | `supported` [c] | `documented` [g] | `documented` [k] | `documented` [o] |
| `lower_cap` | `declarable` | `supported` [d] | `—` [h] | `—` [l] | `documented` [p] |
| objective: minimise total arc cost | `declarable` [q] | `supported` [q] | `documented` [r] | `documented` [s] | `documented` [t] |

Evidence:

- [a] `tests/network/network_model_test.cpp` "NetworkModel solves a simple minimum-cost flow":
  supply 5 / demand −5 over a two-arc path returns `cost() == 10`, which holds only if all 5
  units are routed.
- [b] Same test: a direct arc of cost 5 is available and the returned cost proves the
  cost-1 path was used instead.
- [c] `tests/network/network_model_test.cpp` "NetworkModel respects arc upper capacity": a
  cheap arc capped at 2 forces 2 of 4 units onto a cost-5 arc; returned cost 12, not 4.
- [d] `tests/network/network_model_test.cpp` "NetworkModel respects arc lower bounds": a
  mandatory unit on a cost-5 detour makes the optimum 6 where the unconstrained optimum is 2.
  `McfSolver::solve()` seeds every arc at its lower bound and the residual backward arc is
  bounded by `flow − lower_cap`, so the bound holds for the returned flow.
- [e] `SetNodeSupply(node, supply)`; a demand is a negative supply. `Solve()` requires total
  supply minus total demand to be zero and returns `INFEASIBLE`/`UNBALANCED` otherwise —
  see §Rulings for what that means for #179.
- [f] `AddArcWithCapacityAndUnitCost(tail, head, capacity, unit_cost)`; the unit cost may be
  any integer, negative included.
- [g] Same call: `capacity` is the arc's upper bound.
- [h] The simple API has no arc lower bound — capacity is an upper bound and the implicit
  lower bound is 0. #179 must transform or restrict; the ruling is recorded there.
- [i] Commodity `k` routes `d_k` from `o_k` to `t_k`; a single supply/demand pair is `|K| = 1`.
- [j] Arc cost `c_a` in both the arc-flow and path formulations.
- [k] Arc capacity `u_a` is the coupling constraint of both formulations
  (`sum_k x^k_a <= u_a`).
- [l] Neither formulation carries an arc lower bound: the master couples on `u_a` only, and a
  lower bound is an extra coupling row plus branching, not something the MCF master expresses.
- [m] Node balance is a row with `row_lower_ == row_upper_ == supply`.
- [n] `col_cost_` on the arc's flow column, `ObjSense::kMinimize`.
- [o] `col_upper_` on the arc's flow column.
- [p] `col_lower_` on the arc's flow column.
- [q] Implicit and unique: declaring arc costs declares the objective, there is no setter and
  no second objective. `Result::cost` is `NetworkSolution::cost()`, the sum of `flow * cost`
  over arcs, and every cost assertion in [a]–[d] is an assertion on it.
- [r] `OptimalCost()` after `Solve()` returns `OPTIMAL` — the same objective.
- [s] `min sum_k sum_a c_a x^k_a`.
- [t] Minimise `col_cost_ . x`.

### Result

Primary: **arc flows**. `McfSolver` produces a flow per arc (`NetworkSolution::flow(a)`), and
so would an OR-Tools plugin (`Flow(arc)`); it is the exact object, and the only thing that lets
a third party check a returned solution against `supply`, `lower_cap` and `upper_cap`.

Derived: **path decomposition**, `decompose_paths()` in `src/model/network_model.cpp`.

What a returned `Result` carries today, and the two findings:

| field | contents |
|---|---|
| `cost()` | the declared objective's value, `sum_a flow(a) * cost(a)` |
| `feasible()` | conservation at every node, `lower_cap <= flow <= upper_cap` on every arc, and `resource_feasible()` — vacuous now that no resource is declarable |
| `flows()` | one entry, the greedy path decomposition |
| `work_ticks()` / `work_units()` | deterministic work count |

1. **`Result::flows_` is commodity-indexed** (`src/model/types.h`, "Network / Flow":
   `flows_[commodity]` is a vector of `PathFlow`) while the model has no commodity, so
   `solve()` pushes exactly one entry and `flows()[0]` is the whole answer. Recorded, not
   changed: that indexing is the shape #184 needs.
2. **The decomposition is lossy and the arc flows never leave the engine.** `decompose_paths`
   walks greedily from a supply node and stops when it revisits one (`if (seen[next]) break;`),
   and the outer loop aborts the first time a walk yields no arcs — flow on a cycle, or flow a
   partial walk cannot reach, is not reported. `Result` has no arc-flow field at all, which is
   why the `supported` cells above are evidenced through `cost()` against a hand-computed
   optimum rather than against the flow. Filed as #206.

### Variants

| claim | features needed | verdict |
|---|---|---|
| README "Network flow" | supply, arc cost, `lower_cap`, `upper_cap` | expressible now, but the row name overclaims — narrowed to "Min-cost flow (single commodity)" in the same commit |
| transportation / transshipment / assignment problem | as above | expressible now: special cases of min-cost flow |
| max flow | a throughput objective | not expressible: the only objective is min cost, and an over-supplied instance comes back infeasible rather than maximised |
| N2 RCMCF (#163) | per-path resource budgets, RCSPP pricing | `cut` from this model — see §Rulings, #195. It is column generation, and lands with #184 |
| multi-commodity flow (#184) | commodities | after #184's `extend` |
| fixed-charge network design (#184) | per-arc fixed cost and open decision | after #184's `extend` |
| N3 LSNDP | commodities, design, per-commodity admissibility | after #184's `extend` |
| N4 k-splittable | commodities, path-count limit per commodity | after #184's `extend` |
| UFLP / CFLP | design arcs from a super-source | `cut` as an archetype; reduction shown in §Rulings |

### Rulings

**#195 — `delete` the resource API.** `add_resource()` / `set_resource_usage()` are gone from
`src/model/network_model.h`, `src/model/network_model.cpp`, the `NetworkModel` block of
`python/bindings.cpp`, and `tests/model/model_test.cpp`. They were `drops` on the only column
that could enforce them: `McfSolver` never reads a resource, the operators in
`src/network/network_operators.cpp` are resource-blind too, and `NetworkModel::solve()` never
calls them — so the code was **dead on the model path**, with no owner committed to wiring it.
Under the deletion rule that is a delete, and RCMCF is #184's column generation with RCSPP
pricing rather than a flag on the single-commodity model.

*The cut stops at the model layer.* `NetworkData` keeps its resource fields, and so do
`NetworkSolution::resource_feasible()` and the tests in `tests/network/network_test.cpp`
("NetworkData with resources", the `resource consumption` section of "Shipping network MCF"):
those exercise the engine layer, which E2 does not rule on — nothing there is declarable any
more, so nothing there can mislead a user of the model API, and #184 decides whether that
plumbing is reused or removed. What was stale is fixed in the same commit:
`src/network/mcf_solver.h` no longer advertises "for RCMCF, combine with the network
operators", since the operators enforce nothing either.

**#179's premise is confirmed.** With resources gone the model is exactly single-commodity
minimum-cost flow, and `SimpleMinCostFlow` is an exact oracle for it: same objective, same
supply semantics, same arc capacity. Two gaps the plugin must close, both recorded on #179:

- `lower_cap`: **transform, do not restrict.** For an arc `(i, j)` with bounds `[l, u]`, give
  OR-Tools capacity `u − l`, shift supplies by `b(i) −= l` and `b(j) += l`, and add the
  constant `l * cost` to the reported objective; map the returned flow back with `+ l`. This is
  exact and keeps every declarable instance solvable, where restricting to `lower_cap == 0`
  would reject models the native engine already solves.
- Unbalanced supply: `Solve()` requires total supply to equal total demand. `McfSolver` accepts
  an imbalance and reports it as infeasible through `excess`. The plugin should reject the
  imbalance up front, not silently call `SolveMaxFlowWithMinCost()` — that answers a different
  question.

**Facility location is `cut` as an archetype, and the reduction is a demonstration.** On the
design API sketched below, with facilities `F`, customers `C`, demand `d_j` and assignment
cost `c_ij`:

```text
nodes:   s (super-source), one node per facility i, one node per customer j
supply:  supply(s) = sum_j d_j ;  supply(j) = -d_j ;  supply(i) = 0
arcs:    s -> i   design arc: cost 0, fixed cost f_i, upper_cap = sum_j d_j
         i -> j   assignment arc: cost c_ij, upper_cap = d_j
```

Minimising total cost over that graph is exactly `min sum_i f_i y_i + sum_ij c_ij x_ij` with
`x_ij > 0` only when `y_i = 1`, because the only way into facility `i` is through its design
arc — UFLP. **CFLP** is the same graph with the design arc's `upper_cap` set to the facility's
capacity `u_i`, or with a capacity module list where the facility's capacity is modular.
Single-sourcing (each customer served by one facility) is one commodity per customer with a
path-count limit of 1. No archetype, no new model — the reduction is the ruling.

**#184's capability list — `extend`, additive.** Every row below leaves the current schema and
accessors valid, so #176's introspection and #179's plugin survive it: a model with no
`add_commodity` call is the single-commodity model defined by node supplies, an arc with no
fixed cost is always open, and an arc with no module list has the capacity its `upper_cap`
gives it. The sketch:

```cpp
// 1. Commodities. Node supply is the |K| = 1 special case.
int  add_commodity(int origin, int destination, int demand);

// 2. Design: per-arc fixed cost and open decision, plus capacity modules.
void set_arc_fixed_cost(int arc, int fixed_cost);
void set_pre_installed_capacity(int arc, int capacity, int cost);
void add_capacity_module(int arc, int capacity, int cost);   // repeatable

// 3. Per-commodity admissibility and cost.
void set_arc_commodity_cost(int arc, int commodity, int cost);
void forbid_arc(int arc, int commodity);
void add_admissible_path(int commodity, std::vector<int> arcs);

// 4. Path-count and hop limits.
void set_max_paths(int commodity, int k);          // k == 1 is unsplittable
void set_max_path_length(int commodity, int hops);

// 5. Shared-capacity link groups, for undirected and bidirected links.
int  add_link(int node_a, int node_b);             // one capacity and one open decision
void add_link_arc(int link, int arc);              //   shared by the arcs in the group
```

`Result` gains, per the same rule: per-commodity arc flows (primary) and path flows (derived),
the set of open arcs, and the module count installed per arc.

*Cross-checked against what the instance files carry, not the literature.* SNDlib's native
format (`sndlib-networks-native`, version 1.0) and the model files shipped with
`sndlib-instances-native`:

- `LINKS`: `<id> ( <source> <target> ) <pre_installed_capacity> <pre_installed_capacity_cost>
  <routing_cost> <setup_cost> ( {<module_capacity> <module_cost>}* )` — pre-installed capacity,
  a per-link setup (fixed) cost and a module list are all in the file, and instances such as
  `ta1` carry two modules per link. This is rows 2 above, field for field.
- `DEMANDS`: `<id> ( <source> <target> ) <routing_unit> <demand_value> <max_path_length>` —
  origin, destination, demand: row 1. `max_path_length` is a **hop** limit (values 2, 3, 20 and
  `UNLIMITED` occur), not a path-count limit: row 4's `set_max_path_length`.
- `ADMISSIBLE_PATHS`: `<demand_id> ( {<path_id> ( <link_id>+ )}+ )`, non-empty in `polska` and
  `pioro40` — row 3's `add_admissible_path`. Note what is **not** there: no per-commodity arc
  cost. `routing_cost` is per link, so `set_arc_commodity_cost` is LSNDP literature, not SNDlib
  spec, and #184 should treat it as the weakest-evidenced row of the five.
- The model files pin the rest: `ROUTING_MODEL` ∈ {`CONTINUOUS`, `INTEGER`, `SINGLE_PATH`} —
  splittable, integral, and unsplittable; general k-splittable is **not** an SNDlib parameter,
  so N4's `k` is literature beyond `k == 1`. `LINK_CAPACITY_MODEL` ∈
  {`MODULAR_LINK_CAPACITIES`, `SINGLE_MODULAR_CAPACITIES`, `EXPLICIT_LINK_CAPACITIES`,
  `LINEAR_LINK_CAPACITIES`}, `FIXED_CHARGE_MODEL` ∈ {`YES`, `NO`},
  `ADMISSIBLE_PATH_MODEL` ∈ {`ALL_PATHS`, `EXPLICIT_LIST`}, `HOP_LIMIT_MODEL` ∈
  {`IGNORE_HOP_LIMITS`, `INDIVIDUAL_HOP_LIMITS`}.
- `LINK_MODEL` ∈ {`DIRECTED`, `BIDIRECTED`, `UNDIRECTED`} and `DEMAND_MODEL` ∈ {`DIRECTED`,
  `UNDIRECTED`}. COSO's arcs are directed and independent, and two anti-parallel arcs cannot
  express one capacity and one open decision shared between the directions — hence row 5,
  which the issue's list did not have. `NODE_MODEL` is `NO_NODE_HARDWARE` and
  `OBJECTIVE_MODEL` is `MINIMIZE_TOTAL_COST` throughout, so neither needs a row.
  `SURVIVABILITY_MODEL` ∈ {`NO_SURVIVABILITY`, `SHARED_PATH_PROTECTION`,
  `ONE_PLUS_ONE_PROTECTION`, `UNRESTRICTED_FLOW_RECONFIGURATION`} is real in the files and
  deliberately out of #184's scope — say so there rather than discovering it at benchmark time.

Atamturk's sets (<https://atamturk.ieor.berkeley.edu/data/>) carry a warning rather than a
schema, and it is a sequencing finding for #184 and #177: **they are distributed as MPS files,
not as network instances.** `fixed.charge.network.flow/fc.data.tar.gz` is
`fc.<nodes>.<density>.<seed>.mps.gz`, where the structure survives only in the names — binary
`x.<arc>.1` with the arc's fixed cost in the objective, continuous `y.<arc>` flow with **no**
per-unit cost, variable-upper-bound rows `AVUB<arc>` (`y_a − u_a x_a <= 0`) giving the arc
capacity, and node-balance rows `DEM<node>` typed `E`, `G` **or** `L` — a range, where COSO's
`supply` is an equality. The `.1` index is a commodity index, and it is always 1: these are
single-commodity fixed-charge instances. `arc.set` (splittable and unsplittable capacitated
network design) and `cut.set` are MPS with fully anonymous names (`C0001`, `R0001`), so nothing
structural is recoverable at all. A structural model API can be benchmarked on `fc` only
through a name-parsing converter, and on `arc.set` / `cut.set` not at all — those compare
formulations, which is HiGHS's column, not the model's.

**README.** The "Network flow" row named a family the model does not cover. Narrowed in the
same commit to "Min-cost flow (single commodity)"; the engine-status row already said the
target scope is multi-commodity flow and network design and needed no change.

### Defects

| issue | verdict |
|---|---|
| #195 `NetworkModel` accepts resource constraints that `McfSolver` ignores | `delete` — the resource API is gone from the model layer; issue closed by this ruling. The interim "`solve()` throws" step in that issue is superseded and was not implemented |
| #206 `Result` returns only a lossy path decomposition, never arc flows | filed by this audit; it is what stops the `supported` cells being evidenced on the returned flow rather than on `cost()`. Belongs with #176's `Result` contract |

The native column has no `drops` cell left, which is the condition #179's milestone needs.

## Packing

`PackingModel` (`src/model/packing_model.h`) declares bin types — N-dimensional capacity, a
per-bin cost, an optional count limit — and items with an N-dimensional size, plus pairwise item
conflicts, and solves it as bin packing under one objective: minimise the total cost of the bins
used. The `minimize_bins()` setter is deleted as of this section — see §Rulings.

The N dimensions are **additive resources** (weight, volume, ...), not spatial axes. Nothing in
the schema carries a coordinate; §Rulings turns that into the 2-D / 3-D ruling.

Engine columns are #173's map for this model: **native** (FFD-style construction plus a
merge/move descent, all of it inside `PackingModel::solve()`) and **HiGHS** (#182). Only the
native engine is integrated; HiGHS is not a dependency of this repo, so its column can hold no
value but `documented`, pinned to a public API reference:

- HiGHS v1.7.2, `HighsLp` (`col_cost_`, `col_lower_`, `col_upper_`, `integrality_` —
  `HighsVarType::kInteger` — `a_matrix_`, `row_lower_`, `row_upper_`, `sense_`), passed with
  `Highs::passModel` and solved with `Highs::run()`. #182 names two formulations, arc-flow and
  assignment-based ILP. Every HiGHS cell below is evidenced on the **assignment-based** one
  (`x_ib` = item `i` in bin `b`, `y_b` = bin `b` open), because arc-flow is one-dimensional and
  carries neither N-dimensional capacity nor conflicts.

### Schema

| entity | fields |
|---|---|
| bin type | `capacity` (one int per dimension), `cost` (int per bin used, default 1), `count` (int, 0 = unlimited) |
| item | `size` (one int per dimension, matching the bin capacity dimensions) |
| conflict | an unordered pair of item ids: the two items may not share a bin |

`num_dimensions()` is inferred from the first entity added; `add_bin_type` and `add_item` throw
`std::invalid_argument` on an empty capacity/size and on a dimension count that differs from it.
`add_conflict` throws `std::out_of_range` on an unknown item id and `std::invalid_argument` on a
self-conflict. `solve(TimeLimit)` is the whole call surface — there is no reference solution and
no warm start, so principle 4 has nothing to rule here. A model with no bin type or no item
returns a default-constructed `Result` (`feasible() == false`) rather than throwing.

Absent, and named because the rulings below turn on them: item value, item multiplicity
(demand), item–bin-type compatibility, and item geometry.

### Features

| feature | model | native | HiGHS |
|---|---|---|---|
| bin capacity, N dimensions | `declarable` | `supported` [a] | `documented` [g] |
| bin cost | `declarable` | `supported` [b] | `documented` [h] |
| bin count per type | `declarable` | `supported` [c] | `documented` [i] |
| item size, N dimensions | `declarable` | `supported` [d] | `documented` [j] |
| pairwise item conflicts | `declarable` | `supported` [e] | `documented` [k] |
| objective: minimise total bin cost | `declarable` [f] | `supported` [f] | `documented` [l] |
| item value | `absent` | `—` [m] | `—` [m] |
| item multiplicity (demand) | `absent` | `—` [n] | `—` [n] |
| item–bin-type compatibility | `absent` | `—` [o] | `—` [o] |
| item geometry (2-D / 3-D shape and placement) | `absent` | `—` [p] | `—` [p] |

The four `absent` rows have no engine value to hold: an engine cannot enforce, reject or drop a
declaration the API cannot make. Their `—` cells say why the model cannot express the feature,
and §Rulings says whether that is an `extend` or a `cut`.

Evidence:

- [a] `tests/packing/packing_model_test.cpp` "PackingModel: vector bin packing respects every
  dimension": bins of capacity `{10, 10}` and items `{5,8}`, `{5,8}`, `{5,1}` come back as two
  bins with the first two items separated, and the test recomputes each returned bin's load in
  both dimensions from the declared sizes and asserts it is within capacity. The control
  section declares dimension 0 alone and gets those two items in *one* bin, so the assertion
  fails if the second dimension is ignored. Enforced by the `D`-loop in
  `PackingSolution::item_fits_capacity` and by the same loop in the move and merge feasibility
  checks — the only two `solve()` reaches, via `enumerate_moves` and `enumerate_merges`;
  `enumerate_swaps` and `enumerate_splits` enforce nothing on the model path. And **not** by
  `src/packing/bin_capacity.h`, the file #168 named: nothing on the model path constructs a
  `BinCapacity`, only `tests/packing/bin_capacity_test.cpp` does.
- [b] `tests/packing/packing_model_test.cpp` "PackingModel: variable-sized bin packing costs the
  mix": a large type (capacity 10, cost 5) declared first and a small one (capacity 5, cost 1)
  second, with items 8, 5, 5, return three bins costing 7. Declaration order carries the
  evidence: construction breaks ties on lowest slot index, so an engine that ignored the
  declared costs would fill the large bins and return two bins costing 10. Verified by mutation
  — replacing `assign_cost_delta` / `move_cost_delta` with unit costs, which makes the search
  minimise bin *count*, fails this assertion at 2 bins.
- [c] `tests/packing/packing_model_test.cpp` "PackingModel: bin count limits the slots of a
  type": one type with `count = 1` and three items that each fill a bin returns exactly one bin,
  two unassigned items and `feasible() == false`. Enforced by slot allocation in the
  `PackingSolution` constructor — `count` slots for a limited type, `num_items` for an unlimited
  one.
- [d] The per-bin load recomputation in [a] is an assertion on the declared sizes: it is the
  only thing that ties a returned bin's contents to the numbers the model was given. [b] and [e]
  add the one-dimensional case, where two size-5 items share a capacity-10 bin and a size-8 item
  cannot enter a capacity-5 one.
- [e] `tests/packing/packing_model_test.cpp` "PackingModel: bin packing with conflicts keeps the
  pair apart": two size-5 items and a capacity-10 bin come back in one bin in the control
  section and in two bins, in different bins, once the conflict is declared. Enforced by
  `PackingSolution::has_conflict_in_bin` through `item_fits`, and by the conflict checks in the
  `is_feasible` of the two operators `solve()` reaches.
- [f] Implicit and unique: declaring bin costs declares the objective, and after the
  `minimize_bins()` deletion there is no setter and no second objective. `Result::cost` is
  `PackingSolution::cost()`, **the sum of `bin_cost` over non-empty bins** — maintained
  incrementally as bins open and close in `assign` / `unassign` / `move`, and equal to the bin
  count exactly when every declared `cost` is the default 1. [b]'s `cost() == 7` over bin costs
  1 and 5 is an assertion on the weighted sum, not on the three bins returned.
- [g] One row per bin and dimension: `sum_i s_id x_ib − C_td y_b <= 0`, i.e. `a_matrix_` entries
  with `row_upper_ = 0` and `row_lower_ = −kHighsInf`. Dimensions add rows, not columns, so N is
  unbounded.
- [h] `col_cost_[y_b] = cost(t)` for the bin's type `t`.
- [i] Either generate only `count_t` bin columns of type `t`, or add the cardinality row
  `sum_{b of type t} y_b <= count_t` with `row_upper_ = count_t`.
- [j] The `s_id` coefficients of [g], plus one assignment row per item,
  `sum_b x_ib = 1` (`row_lower_ = row_upper_ = 1`).
- [k] `x_ib + x_jb <= 1` for each conflicting pair `(i, j)` and each bin `b`
  (`row_upper_ = 1`). All `x` and `y` columns are `integrality_ = HighsVarType::kInteger` with
  bounds `[0, 1]`.
- [l] `min col_cost_ . x` with `sense_ = ObjSense::kMinimize`; `Highs::getSolution()` returns
  the `y_b` values that price it.
- [m] `ItemParams` has one field, `size`. There is no value and no objective that could read one
  — see §Rulings, knapsack.
- [n] An item is one object; a demand of 500 identical pieces is 500 `add_item` calls — see
  §Rulings, cutting stock.
- [o] Any item may enter any bin type whose capacity holds it. There is no per-(item, bin type)
  admissibility flag, so "this item only fits refrigerated bins" is inexpressible except by
  giving the forbidden types a capacity the item overflows in some dimension — a modelling
  trick, not a declaration.
- [p] An item is a vector of additive loads; a 2-D or 3-D item is a shape and its solution is a
  *placement*. Neither the schema nor `Result` carries a coordinate — see §Rulings, 2-D / 3-D.

### Result

Primary: **the item ids in each bin, and the bin's type**. The partition alone is not enough to
verify a `supported` cell whenever more than one bin type is declared.

What a returned `Result` carries today:

| field | contents |
|---|---|
| `cost()` | the declared objective's value, `sum over non-empty bins b of bin_cost(bin_type(b))` |
| `feasible()` | no capacity violation, no conflict violation, **and** every item assigned (`PackingSolution::feasible()`) |
| `bins()` | one entry per non-empty bin, holding that bin's item ids; empty slots are dropped |
| `num_bins()` | `bins().size()` — non-empty bins only, so it is the number of bins *used* |
| `unassigned()` | the items no bin could take |
| `iterations()`, `work_ticks()` / `work_units()`, `elapsed_seconds()` | search and work counters |

Two findings:

1. **Bin type per bin is not returned.** `Result::bins_` is `vector<vector<int>>` — item ids and
   nothing else. With a single bin type the type is recoverable by construction, which is why
   the 1-D cells above can be evidenced; with **heterogeneous bin types a third party cannot
   cost or check the returned solution**, because a bin holding 5 units of load may be a
   capacity-5 bin costing 1 or a capacity-10 bin costing 5, and only the engine knows which.
   `cost()` cannot be recomputed from `bins()`, and no capacity check is possible either. The
   variable-sized test above therefore asserts `cost()` and the bin count, and identifies the
   large bin only indirectly — the size-8 item fits no other type. Filed as #207; it belongs
   with #176's `Result` contract, next to #206.
2. **`Result::unassigned_` reads as assignment-only.** In `src/model/types.h` the field sits in
   the "Assignment (nurse rostering)" block and carries no comment of its own, and the class
   header comment lists `unassigned()` under neither assignment nor packing. But
   `PackingModel::solve()` populates it, and packing has no other channel for unpacked items —
   [c]'s two unassigned items arrive there. The placement is the documentation, and it is
   wrong. Noted on #176.

### Variants

| claim | features needed | verdict |
|---|---|---|
| K1 BPP (#167, README "Bin packing") | one bin type, item size, unit bin cost | expressible now: evidence [a]–[f], and "PackingModel: simple 1D instance end-to-end" solves a 7-item instance through `solve()` and checks the returned bin count against `continuous_lower_bound()` |
| K2 vector bin packing (#168) | N-dimensional capacity and size | expressible now: [a]. The model-level gap the closed issue left open is closed by that test — and the multi-dimensional code it credits, `bin_capacity.{h,cpp}`, is not what enforces it |
| K3 bin packing with conflicts (#169) | pairwise conflicts | expressible now: [e] |
| variable-sized BPP | several bin types with different capacity and cost | expressible now: [b]. `count` bounds the supply of each size |
| bin packing with fixed bin supply | bin `count` | expressible now: [c]. Excess items come back in `unassigned()`, with `feasible() == false` |
| cutting stock, 1-D | item multiplicity | after an `extend` — §Rulings, #208 |
| knapsack, single and multi-dimensional | item value, a maximise-value objective | `cut` — §Rulings |
| 2-D / 3-D bin packing, strip packing, pallet and container loading | geometry: shapes, coordinates, rotation | `cut` — §Rulings |
| bin packing with item–bin compatibility | per-(item, bin type) admissibility | not expressible; no owner. [o] |

### Rulings

**`minimize_bins()` — `delete`.** The setter and its `minimize_bins_` field are gone from
`src/model/packing_model.h` and `src/model/packing_model.cpp`, with the four call sites updated
(`tests/model/model_test.cpp`, `tests/packing/packing_model_test.cpp` ×2,
`examples/canonical/packing_example.cpp`). The flag was written and read nowhere:
`PackingModel::solve()` always minimises `PackingSolution::cost()`, the weighted bin cost, and
would do so whether or not the setter had been called. Under the deletion rule that is a delete
— no engine column can be `supported` for an objective no engine distinguishes, and there is no
dormant "count the bins" objective anywhere to wire in. The objective survives as the implicit
one, exactly as arc cost is in `NetworkModel`: declaring `cost` on the bin types declares it,
and leaving `cost` at its default 1 makes it bin minimisation. This is a cut, so it is E2 work
(principle 5); #171's closed description mentions `minimize_bins` in the binding it never added,
which is now one thing less to bind.

**Cutting stock, 1-D — `extend` via item multiplicity, filed as #208.** CSP *is* this model with
demands: BPPLIB (Delorme, Iori and Martello, *Optimization Letters* 12(2):235–250, 2018) ships
every instance in both a BPP and a CSP version, and the CSP one adds exactly one field —
`BPPLib.jl`'s `CSPData` is `name`, `capacity`, `weights`, **`demands`**, `lb`, `ub` against
`BPPData`'s same list without `demands`. (The published `.txt` files themselves could not be
inspected: `or.dei.unibo.it/library/bpplib` now 302s to a site index, and
`tests/data/download_benchmarks.sh` has no BPP or CSP entry to read one from. The field list
above is the documented schema, not a line format read off an instance.) The workaround today is
to repeat the item: `add_item` once per unit of demand. That is expressible, and it is why this
is an `extend` rather than a defect — but it is quadratic in exactly the wrong place, since CSP
demands run to the hundreds or thousands per size and every pairwise conflict check, move
enumeration and merge enumeration in the native engine is over items, not distinct sizes. It
also destroys the structure the engine most wants: identical items become distinguishable, so
the search wastes its time on symmetric solutions. And #182's arc-flow formulation consumes
multiplicity natively — a demand row per distinct size is *the* CSP formulation (Valério de
Carvalho), so the `extend` costs that engine nothing and the repeat-the-item workaround costs it
its whole advantage. #208 sketches `ItemParams::count` and an `item_count(i)` accessor, additive
per principle 5, landing in #182.

**Knapsack — `cut`.** A knapsack declares an item *value* and one capacitated container and asks
for the maximum-value subset that fits. Two things are missing and neither is a field: there is
no item value ([m]), and the objective is minimise bin cost, which for a single bin is a
constant — every subset that fits scores the same, so `solve()` would return an arbitrary
feasible packing and call it optimal. Making it work means a second objective on a model whose
objective is unique, and a *maximisation* at that. The reason to add none of it is that nothing
structure-aware is planned: #173's engine map has no knapsack oracle, HiGHS solves it exactly
and the textbook DP solves it in pseudo-polynomial time. Note where a knapsack legitimately
appears — as the pricing subproblem of a column-generation CSP or BPP solver. That is inside an
engine, not a declaration a user makes, and #182 is free to solve as many knapsacks as it likes.

**2-D / 3-D packing — `cut`.** Geometry is a different algorithm family. An item here is a
vector of additive loads; a 2-D or 3-D item is a shape, and the answer is a **placement** —
coordinates, orientation, and a cutting pattern (guillotine or free) — not a partition of the
items. Nothing in the schema carries a coordinate and `Result::bins_` cannot return one, so the
cut is structural rather than a matter of missing fields: the whole solution object is the wrong
shape. The engine map is the second reason. The native operators (merge, move, swap, split)
reason only about summed load vectors, and additivity is exactly the property geometry lacks —
four 5×5 items "fit" a 10×10 bin by area *and* would pass a two-dimensional vector-capacity
check at (10, 10) totals, while a fifth 1×10 item passing neither check is what the real problem
is about; conversely a set that passes an area check may have no feasible placement at all.
HiGHS can formulate 2-D bin packing, but only with placement or relative-position variables,
which is a different model from [g]–[l] and a weak one. #173 lists no geometric oracle to plug
in. Cut, and the N-dimensional capacity vector must not be sold as 2-D/3-D packing: it is vector
packing, K2.

### Defects

| issue | verdict |
|---|---|
| #207 packing `Result` does not say which bin type each returned bin is | filed by this audit. It is what stops the heterogeneous-bin cells being evidenced on the returned solution rather than on `cost()`. Belongs with #176's `Result` contract |
| #208 `ItemParams` has no multiplicity, so cutting stock is one `add_item` per unit of demand | filed by this audit as the cutting-stock `extend`; lands in #182 |
| `Result::unassigned_` documented as assignment-only, populated by packing | noted on #176; a comment fix on a shared field, not a schema change |

Two findings with no issue of their own, both for #182:

- **`packing_benchmark_test` does not go through the model API**, contrary to what this audit's
  issue assumed. It builds a `PackingModel` only to call `PackingData::build`, then runs its own
  `first_fit_decreasing` and `local_search` over `PackingSolution` —
  `PackingModel::solve()` is never called, and the two implementations are not the same search.
  Under the evidence rule none of it is evidence for a `supported` cell, which is why every cell
  above cites `packing_model_test.cpp` instead. Its instances are hand-written arrays named
  after Falkenauer and Scholl classes, not the published files: `download_benchmarks.sh` has no
  BPP entry. #182's step 3 is where this becomes real benchmark coverage; until then "tested
  against Falkenauer" (#167, #172) overstates what runs. README does not use that phrase, but
  makes its own wrong claim in the same direction: `packing_benchmark_test` is listed among the
  executables that "run against instances fetched by `tests/data/download_benchmarks.sh`", and
  no fetched instance reaches it.
- **#170 (BPPLIB parser) and #171 (Python bindings for `PackingModel`) are closed as completed,
  and neither exists in the tree.** There is no packing parser anywhere in `src/`, and
  `python/bindings.cpp` binds `RoutingModel`, `NetworkModel` and `LotSizingModel` only. Nothing
  in this audit depends on either, and no Python surface is affected by the cut above — but
  their closed state is not a record of work done, and #182 should not plan around them.

## Lot sizing

`LotSizingModel` (`src/model/lotsizing_model.h`, 52 lines) declares a planning horizon, products
carrying a setup cost, a setup time, a unit production cost and a holding cost, an external demand
per product and period, one production capacity per period, and a bill of materials, and solves it
as the capacitated lot-sizing problem. Nothing is cut by this section; the BOM is kept and its
native cell is `drops` — see §Rulings.

Engine columns are #173's map for this model: **native** (`lot_for_lot` / `silver_meal` /
`part_period_balancing` from `src/lotsizing/construction.cpp`, then a shift/merge/split descent
inside `LotSizingModel::solve()`) and **HiGHS** (#183). Only the native engine is integrated;
HiGHS is not a dependency of this repo, so its column can hold no value but `documented`, pinned
to a public API reference:

- HiGHS v1.7.2, `HighsLp` (`col_cost_`, `col_lower_`, `col_upper_`, `integrality_` —
  `HighsVarType::kInteger` — `a_matrix_`, `row_lower_`, `row_upper_`, `sense_`), passed with
  `Highs::passModel` and solved with `Highs::run()`. Every HiGHS cell below is evidenced on the
  **standard aggregate CLSP formulation** — the one #183 names — with, per product `p` and period
  `t`, a continuous production column `x_pt`, a binary setup column `y_pt` and a continuous
  inventory column `s_pt`:

```text
balance   s_{p,t-1} + x_pt - s_pt = d_pt                       (s_{p,-1} = 0)
setup     x_pt - M_pt y_pt <= 0,   M_pt = min(sum_{u >= t} d_pu, C_t - st_p)
capacity  sum_p (x_pt + st_p y_pt) <= C_t
min       sum_{p,t} (sc_p y_pt + uc_p x_pt + hc_p s_pt)
```

### Schema

| entity | fields |
|---|---|
| horizon | `num_periods` (int > 0) |
| product | `setup_cost`, `setup_time`, `unit_production_cost`, `holding_cost` (double, one value per product, constant over the horizon) |
| demand | `demand[product][period]` (double, default 0) |
| capacity | `capacity[period]` (double, default **0**) |
| BOM edge | `parent`, `child`, `quantity` (double, default 1.0) |

`set_num_periods` throws `std::invalid_argument` on a non-positive horizon and **resets demand and
capacity**, so it must be called before the values it would wipe. `set_demand` and `set_capacity`
throw `std::out_of_range` on an unknown product or period; `add_bom` throws `std::out_of_range` on
an unknown product and `std::invalid_argument` on `parent == child`, and does not check the BOM
graph for cycles. `solve(TimeLimit)` is the whole call surface — there is no reference solution and
no warm start, so principle 4 has nothing to rule here. A model with no product or no period
returns a default-constructed `Result` (`feasible() == false`) rather than throwing.

Two defaults worth stating because a user hits them first. **Capacity defaults to 0**, so a model
that never calls `set_capacity` cannot produce anything: every plan with production violates
capacity in every period and comes back `feasible() == false`. **Demand defaults to 0**, which is
the harmless direction — an unset product-period simply has nothing to make.

Absent, and named because the rulings below turn on them: initial inventory, backlog as a cost,
setup carry-over between periods, multiple resources or parallel machines, minimum and maximum lot
size, lead times, per-period variation of any of the four cost fields, all-or-nothing production
(DLSP), within-period sequencing (GLSP), and the unit production *time* — `capacity_usage` in
`src/lotsizing/lotsizing_solution.cpp` charges exactly one unit of capacity per unit produced, a
hardcoded rate of 1, plus the product's setup time wherever it produces.

### Features

| feature | model | native | HiGHS |
|---|---|---|---|
| planning horizon (`set_num_periods`) | `declarable` | `supported` [a] | `documented` [k] |
| product (`add_product`) | `declarable` | `supported` [b] | `documented` [l] |
| external demand per product-period (`set_demand`) | `declarable` | `supported` [c] | `documented` [m] |
| production capacity per period (`set_capacity`) | `declarable` | `supported` [d] | `documented` [n] |
| BOM dependent demand (`add_bom`) | `declarable` | `drops` [e] — **dead** | `documented` [o] |
| `setup_cost` | `declarable` | `supported` [f] | `documented` [p] |
| `setup_time` | `declarable` | `supported` [g] | `documented` [q] |
| `unit_production_cost` | `declarable` | `supported` [h] | `documented` [r] |
| `holding_cost` | `declarable` | `supported` [i] | `documented` [s] |
| objective: minimise setup + production + holding cost | `declarable` [j] | `supported` [j] | `documented` [t] |
| initial inventory | `absent` | `—` [u] | `—` [u] |
| backlog as a cost | `absent` | `—` [v] | `—` [v] |
| setup carry-over between periods | `absent` | `—` [w] | `—` [w] |
| multiple resources / parallel machines | `absent` | `—` [x] | `—` [x] |
| minimum and maximum lot size | `absent` | `—` [y] | `—` [y] |
| lead times | `absent` | `—` [z] | `—` [z] |
| per-period cost variation | `absent` | `—` [aa] | `—` [aa] |
| all-or-nothing production (DLSP) | `absent` | `—` [ab] | `—` [ab] |
| within-period sequencing (GLSP) | `absent` | `—` [ac] | `—` [ac] |
| unit production time | `absent` | `—` [ad] | `—` [ad] |

The ten `absent` rows have no engine value to hold: an engine cannot enforce, reject or drop a
declaration the API cannot make. Their `—` cells say why the model cannot express the feature, and
§Rulings says whether that is an `extend` or a `cut`.

Every native `supported` cell is evidenced on one test,
`tests/lotsizing/lotsizing_model_test.cpp` "LotSizingModel: CLSP with capacity binding in one
period", so it is worth stating the instance once. Two products over three periods, demand 10 per
product per period:

| product | `setup_cost` | `setup_time` | `unit_production_cost` | `holding_cost` |
|---|---|---|---|---|
| A | 50 | 2 | 1 | 1 |
| B | 60 | 3 | 2 | 1 |

Total production is fixed at 30 per product — no initial inventory, no backlog, and
overproduction only costs — so the variable production cost is the constant `30 * 1 + 30 * 2 = 90`
and the plan is decided by setups against holding. Relaxed, the optimum is one setup per product in
period 0: `setups 110 + holding 60 + production 90 = 260`, using `30 + 30 + 2 + 3 = 65` units of
capacity in period 0. With capacity `(25, 100, 100)` that plan is out and the optimum becomes
`A = (10, 20, 0)`, `B = (10, 20, 0)`: `setups 220 + holding 20 + production 90 = 330`, with
period-0 usage `10 + 10 + 2 + 3 = 25`, exactly the declared capacity and the only binding period.
Both optima are **unique**, established by exhaustive enumeration over a 0.25-unit production grid
rather than by inspection. The test asserts the returned `production()` and `inventory()` matrices
entry by entry, recomputes the inventory balance from the returned production and the declared
demand, recomputes each period's capacity usage from the returned production and the declared setup
times, and asserts `cost() == 330`.

Evidence:

- [a] Three periods are declared and the returned `production()[p]` and `inventory()[p]` are three
  long, with the inventory balance chained across exactly those three periods — the horizon is what
  the plan spans, not a stored number.
- [b] Two products are declared and come back as two independent rows, each with its own plan and
  its own cost contribution. Their plans coincide here because the instance is symmetric in
  quantity; [f]–[i] are the four fields that price them differently.
- [c] The test recomputes `inventory[p][t]` as `inventory[p][t-1] + production[p][t] - demand[p][t]`
  from the *declared* demand and asserts it equals the returned inventory, and that it is
  non-negative — the only assertion that ties the returned plan to the numbers the model was given.
  Demand also fixes the totals: 30 per product in every section.
- [d] The capacitated section returns 330 and the control section — the same declaration with
  period 0's capacity raised to 1000 and nothing else changed — returns 260 with a different plan.
  Recomputed period-0 usage is 25 against a declared 25 in the first and 65 against 1000 in the
  second. An engine that ignored capacity would return the control's answer in both. Two separate
  mechanisms enforce it, and neither is the other: `is_feasible()` in
  `src/lotsizing/lotsizing_operators.cpp` does its own single-period check on the move's
  `to_period`, while `has_capacity_violation()` is what backs `Result::feasible()`. Neither is the
  constructions, which never read `capacity()` at all — see §Defects, #211, for what that costs.
- [e] `add_bom` is stored, copied into `LotsizingData` and never read again.
  `LotsizingSolution::recompute_inventory_` balances external demand only — its own comment claims
  the constructions and operators handle dependent demand, and neither mentions the BOM.
  `is_multi_level()`, `children()`, `parents()` and `is_end_product()` have exactly one caller in
  the tree, `tests/lotsizing/lotsizing_test.cpp`, which asserts the adjacency lists at the data
  layer and never solves. A parent with demand 5 per period and `add_bom(parent, child, 2.0)`
  returns `production[child] = [0, 0, 0]` and `feasible() == true`. **Dead**, not dormant: there is
  no idle dependent-demand code anywhere to wire in. Filed as #210, with the
  `SKIP`-ed "LotSizingModel: BOM generates dependent demand for the child" holding the assertion
  that should pass.
- [f] The capacitated optimum runs two setups per product and the control one, and the difference
  between the asserted 330 and 260 is `+110` of setup cost against `-40` of holding. An engine
  ignoring `setup_cost` would return 110, not 330, and would have no reason to prefer the control's
  plan.
- [g] Third section: capacity 20 in period 0. Both products carry demand 10 there and there is no
  initial inventory, so every feasible plan sets both up in period 0 —
  `10 + 10 + 2 + 3 = 25 > 20` — and the instance is infeasible; the returned plan's recomputed
  period-0 usage exceeds the declared capacity. The same declaration with the setup times removed
  and nothing else changed fits in 20 exactly and returns the 330 plan. Setup time is the only
  difference between the two, and it changes the returned solution. Enforced by the setup-time term
  in `LotsizingSolution::capacity_usage`.
- [h] A and B carry different unit costs, 1 and 2, over equal total production of 30 each. The
  asserted cost carries `30 * 1 + 30 * 2 = 90` of it; a dropped or shared unit cost gives 240 or a
  wrong total, not 330.
- [i] Holding is 20 in the capacitated section and 60 in the control, on the same declared
  `holding_cost` of 1 per product: it is the whole reason the control's single-setup plan costs
  more to hold and less to set up. Charged on positive inventory only, in
  `LotsizingSolution::recompute_costs`.
- [j] Implicit and unique: declaring the four cost fields declares the objective, there is no
  setter and no second objective. `Result::cost` is `LotsizingSolution::cost()`, which
  `recompute_costs()` maintains as `setup_cost_ + holding_cost_ + production_cost_` — the sum of
  `setup_cost(p)` over set-up product-periods, `holding_cost(p) * inventory` over **positive**
  inventories, and `unit_production_cost(p) * production` over all of them. Every cost assertion
  above is an assertion on it.
- [k] The horizon is the `t` index of the column and row families above; nothing in `HighsLp`
  bounds it.
- [l] The `p` index of the same families. `Highs::passModel` takes one `HighsLp` for all of them.
- [m] `row_lower_ == row_upper_ == d_pt` on the balance row.
- [n] The capacity row, `row_upper_ = C_t` with `row_lower_ = -kHighsInf`.
- [o] One extra term in the balance row it already has: for a BOM edge `(q, p)` with gozinto `r`,
  the child's balance becomes `s_{p,t-1} + x_pt - s_pt - sum_q r_qp x_qt = d_pt`, i.e. an
  `a_matrix_` entry of `-r_qp` on the parent's production column in the child's balance row. No new
  column, no new row — which is why MLCLSP is cheap for #183 and why this cell is `documented`
  while the native one is `drops`.
- [p] `col_cost_[y_pt] = sc_p`, with `integrality_[y_pt] = HighsVarType::kInteger` and bounds
  `[0, 1]`.
- [q] The `st_p` coefficient on `y_pt` in the capacity row, and the `C_t - st_p` term of the setup
  row's `M_pt`.
- [r] `col_cost_[x_pt] = uc_p`.
- [s] `col_cost_[s_pt] = hc_p`. `col_lower_[s_pt] = 0` is what makes backlog an infeasibility here
  too, and what a backlog `extend` would change — see §Rulings, #212.
- [t] `min col_cost_ . x` with `sense_ = ObjSense::kMinimize`; `Highs::getSolution()` returns the
  `x`, `y` and `s` values that price it.
- [u] Every product starts the horizon empty: `recompute_inventory_` takes `prev_inv = 0` at
  `t == 0` and there is no setter. §Rulings, #212 — this is the field the instance files below all
  carry and the model cannot take.
- [v] `ProductEntry` has no backlog cost, and the objective has no term for one. A negative
  inventory is an infeasibility instead — see §Result.
- [w] `setup(p, t)` is a per-period indicator with no link between periods: producing the same
  product in `t` and `t+1` pays `setup_cost(p)` twice and `setup_time(p)` twice, always.
- [x] `capacity(t)` is one number per period. There is no resource index on the capacity, on the
  setup time, or on production, so "two machines each with 400 hours" is inexpressible except as
  one pooled 800 — which is a different problem the moment a lot cannot be split across machines.
- [y] Production is any non-negative real, bounded only by capacity. There is no minimum lot size
  and no maximum other than the period's capacity.
- [z] Production in period `t` is available for demand in period `t`. There is no offset between
  the period a lot is made in and the period it can be used in — which is also what makes a BOM
  lead time inexpressible on top of #210.
- [aa] All four cost fields are one value per product for the whole horizon:
  `LotsizingData::setup_cost(p)`, `setup_time(p)`, `unit_production_cost(p)`, `holding_cost(p)` take
  a product and no period. `capacity(t)` is the only thing that varies over the horizon besides
  demand.
- [ab] Production is continuous on `[0, capacity]`. There is no way to say "produce at full
  capacity or not at all", which is the whole of DLSP.
- [ac] A period is a bucket with a total capacity; nothing in the schema orders what happens inside
  it, and `Result` carries no start time or sequence. Sequence-dependent setup costs and times have
  nowhere to go either.
- [ad] `capacity_usage(t)` adds `production_[idx]` directly: one unit of capacity per unit produced,
  for every product. A product that takes 2 hours per unit while another takes 0.5 cannot be
  declared; the workaround is to scale that product's demand, holding cost and unit cost together,
  which is a modelling trick, not a declaration.

### Result

Primary: **`production[p][t]`**, the quantity of product `p` made in period `t`. Everything else a
third party needs to check a returned CLSP solution against the declaration follows from it —
inventory by the balance, setups by the sign, capacity usage by adding the declared setup times.

Also returned: **`inventory[p][t]`**, the end-of-period inventory. It is derived —
`inventory[p][t] = inventory[p][t-1] + production[p][t] - demand[p][t]` from an empty start — and
it is returned anyway because it is what the holding cost is charged on, and because a caller
should be able to check the balance rather than recompute the engine's own arithmetic and agree
with it by construction. The test does both.

**Setup indicators are derivable and need no field.** `LotsizingSolution::set_production` clamps
the quantity to `max(0.0, qty)` and sets `setup_[idx] = (qty > 0.0)` in the same statement, so
`setup(p, t)` and `production(p, t) > 0` are the same predicate — there is no setup with zero
production anywhere on the model path, and a caller recovers the setup pattern, the setup cost and
the setup-time share of capacity from `production()` alone. This is what the test's capacity
recomputation does.

**Backlog would need its own array.** `inventory_` is one signed array and a negative entry is an
*infeasibility*, not a backlog: see §Rulings, #212. If backlog is extended, the balance splits into
`s_pt - b_pt` and `Result` gains `backlog[p][t]` beside the two arrays it has — inventory alone
cannot carry it, because a period holding stock of one product while backlogging another is normal
and a single signed array would be read as one or the other.

What a returned `Result` carries today:

| field | contents |
|---|---|
| `cost()` | the declared objective's value, `sum_{p,t} (setup_cost(p) [x>0] + unit_production_cost(p) x + holding_cost(p) max(inv, 0))` |
| `feasible()` | `LotsizingSolution::feasible()`: no negative inventory and no period over capacity |
| `production()` | `production_quantities_[product][period]` |
| `inventory()` | `inventory_levels_[product][period]` |
| `iterations()` | accepted descent moves |
| `work_ticks()` / `work_units()`, `elapsed_seconds()` | work and time counters |

Three findings:

1. **Backlog is an infeasibility, not a cost.** `LotsizingSolution::feasible()` is
   `!has_demand_violation() && !has_capacity_violation()`, and `has_demand_violation()` is
   "some `inventory_[p * T + t] < -1e-9`". `total_backlog()` — the sum of the negative inventories
   — exists in `src/lotsizing/lotsizing_solution.{h,cpp}` and has **no caller anywhere but
   `tests/lotsizing/lotsizing_test.cpp`**, and no model-side cost drives it: `recompute_costs()`
   charges holding on positive inventory only and has no backlog term to charge. On the model path
   it is not merely uncosted but always zero, because every construction produces at least the
   period's demand and every move's `is_feasible()` rejects a demand violation. Recorded, not
   changed: it is the shape the #212 `extend` needs, and the ruling is that unmet demand is a
   rejected plan until then.
2. **No cost breakdown reaches the caller.** `LotsizingSolution` maintains `setup_cost()`,
   `holding_cost()` and `production_cost()` separately and `Result` returns only their sum, so a
   caller cannot check the three components independently — the test recomputes them from
   `production()` and the declaration instead. Noted on #176 rather than filed: it is a `Result`
   contract question for every model, not a lot-sizing bug.
3. **`feasible() == false` is returned with a full plan attached.** `solve()` fills
   `production_` and `inventory_` from `best` whether or not `best.feasible()`, so an infeasible
   answer carries the plan that violates the declaration. That is the right shape — it is what lets
   the setup-time section assert *how* the returned plan overruns capacity rather than only that it
   does — but a caller must check `feasible()` before trusting the numbers. See #211 for the reason
   an infeasible answer comes back at all on instances that have a feasible plan.

### Variants

| claim | features needed | verdict |
|---|---|---|
| README "CLSP" (#143) | horizon, products, demand, capacity, setup cost and time, holding | expressible now: the whole §Features evidence list. Bounded by #211 — an instance whose only feasible plans pre-build ahead of a capacity spike comes back `feasible() == false` |
| CLSP with setup times (Trigeiro et al. 1989) | `setup_time` | expressible now: [g]. This is the field-for-field fit checked in §Rulings |
| ULSP / Wagner-Whitin, uncapacitated single-item | horizon, demand, setup and holding cost | expressible now: one product and a capacity above total demand — the control section is exactly this, and returns the Wagner-Whitin answer |
| README "MLCLSP" (#143) | BOM dependent demand | declarable and **dropped** — #210. Not a variant this model expresses today, whatever `is_multi_level()` reports |
| P3 DLSP (#144) | all-or-nothing production, small time buckets | `cut` — §Rulings |
| P4 GLSP (#145) | within-period sequencing, sequence-dependent setups | `cut` — §Rulings, and it is a scope ruling on a single model, not a principle-2 composition |
| P5 CSLP / setup carry-over (#146) | a setup state linked across periods | after an `extend` — §Rulings, #213 |
| P6 lot sizing with backlogging (#147) | backlog cost, a backlog array in `Result` | after an `extend` — §Rulings, #212 |
| P7 multi-machine lot sizing (#148) | a resource index on capacity, setup time and production | `cut` — §Rulings, and it too is a scope ruling on a single model, not a principle-2 composition |
| lot sizing with minimum or maximum lot size | production bounds per product | `cut` — §Rulings |
| lot sizing with lead times | an offset between production and availability | `cut` — §Rulings |
| lot sizing with time-varying costs | per-period cost fields | after an `extend` — §Rulings, #212 |
| inventory routing (MIRPLIB, R22 #125) | a routing solution and a lot-sizing solution joined by an outer loop | `cut` by principle 2 — two models, not one. §Rulings records that #183 and #177 both had it filed as a lot-sizing source |

### Rulings

**Field inventory of the two instance formats.** #183 names Atamtürk's lot-sizing datasets and
MIRPLIB. MIRPLIB is ruled out below; the second set is Trigeiro et al. (1989), the standard
CLSP-with-setup-times benchmark, which #177 does not list. Both were downloaded and read — this
table is what the files carry, not what a paper says they carry.

*Atamtürk & Muñoz, `capacitated.lotsizing`* —
<https://atamturk.ieor.berkeley.edu/data/capacitated.lotsizing/cls.data.tar.gz> (619 KB, HTTP 200),
246 files, 100 instances named `cls.T90.C{2..5}.F{100,200,250,500,1000}.S{1..5}`, from *A Study of
the Lot-Sizing Polytope*, Mathematical Programming 99:443–465 (2004). Instance read:
`cls.T90.C3.F100.S1`, with its `.dat`, its `.mps` twin and the AMPL generator `ls.T90.C3.F100.S1.mod`
that produced both.

*Trigeiro, Thomas & McClain (1989)* — the original Auburn FTP host is dead and has no Wayback
snapshot, and OR-Library's lot-sizing page says outright that it has no lot-sizing instances. The
live distribution is Christopher Sürie's converted set,
<http://www.suerie.de/testsets/clspl/data.tar.bz2> (297 KB, HTTP 200): 751 instances, one directory
of eleven `.PRN` files each, with a published format specification
(`TI_CLSPL_Description.pdf`) and a TTM-name mapping (`conv_ttm_new.txt`). Instance read:
`test0001`, which that mapping identifies as TTM `E1.dat` — 6 items, 15 periods, 1 resource.
Every "in all 751" claim below was checked by reading all 751 instance directories.

| field | Atamtürk `cls.T90.C3.F100.S1` | Trigeiro `test0001` (TTM `E1`) | `LotSizingModel` |
|---|---|---|---|
| number of products | no field — single item | `INDEX.PRN` col 1, `J = 6` | `add_product`, `declarable` |
| number of periods | line 1 of the `.dat`, `T = 90` | `INDEX.PRN` col 2, `T = 15` | `set_num_periods`, `declarable` |
| number of resources | no field — one implied | `INDEX.PRN` col 3, `M = 1` (and `M = 1` in all 751) | **`absent`** — one capacity per period |
| demand per product-period | `.dat` lines 2..T+1, **cumulative**; per-period by differencing | `P-BEDARF.PRN`, J × T matrix | `set_demand`, `declarable` |
| capacity per period | `.dat` lines T+2..2T+1, varies per period (23–37 for `C3`) | `KAPAZ.PRN`, M × T, but **constant over t in all 751** | `set_capacity`, `declarable` |
| setup cost | `.mod` / `.mps` only, **one value per period**, `floor(Uniform(901,1100))` | `RUESTK.PRN`, one per item, time-invariant (25–2000 across the set, no zeros) | `setup_cost`, `declarable` — but per product only |
| setup time | no field | `RUESTZ.PRN`, `(m, j, st)` triples; 5–150, no zeros | `setup_time`, `declarable` |
| unit production cost | `.mod` / `.mps` only, **one value per period**, `floor(Uniform(81,120))` | no field — the spec attributes no direct production cost to a lot | `unit_production_cost`, `declarable` — but per product only |
| holding cost | `.mod` / `.mps` only, 10 per period and **60 in the terminal period** | `LAGKOST.PRN`, one per item, time-invariant | `holding_cost`, `declarable` — but per product only |
| unit production time | no field — implicit 1 | `PRODKOEF.PRN`, `(m, j, a_mj)`; values 0.5–1.5, **not 1 in 10 of the 751** | **`absent`** — hardcoded 1 |
| initial inventory | **no field** | `L0.PRN`, one per item — **present, and 0 in all 751** | **`absent`** |
| required ending inventory | no field | `LT.PRN`, one per item — present, and 0 in all 751 | **`absent`** |
| overtime / soft capacity | no field | `UEBER-KS.PRN`, one per resource — 10000 in all 751 | **`absent`** — capacity is hard |
| BOM / multi-level | no field | `DIREKT-B.PRN`, `(j, k, r_jk)` triples — present but **empty in all 751**; three bytes of DOS line ending | `add_bom`, `declarable` but `drops` — #210 |
| backlog cost | no field; `roof` forbids negative inventory | no field; the CLSPL model is stated as being without backlogging | **`absent`** |
| setup carry-over | no field | not a data field — the same files serve CLSP and CLSPL | **`absent`** |
| min / max lot size | no field; only `size[t] <= producing[t] * capacity[t]` | no field; only the big-M link | **`absent`** |
| lead time | no field | no field — `LT.PRN` is *ending inventory*, not lead time | **`absent`** |

Two expectations of #205 that the files did not bear out, recorded because they change the rulings
below:

- **Initial inventory is not universal.** The issue expected it in both. Atamtürk has no field at
  all, Trigeiro has a dedicated one that is zero in every instance. It stays an `extend` — see
  #214 — but on the strength of rolling-horizon use, not on the strength of the benchmark data.
- **Neither set is multi-level, and Atamtürk is not multi-item.** `DIREKT-B.PRN` is empty in all
  751 and Atamtürk has no product dimension. So the MLCLSP half of the README's claim has no
  benchmark behind it either way, quite apart from #210.

And one that changes #177's sequencing: **neither set can be read into the schema as it stands.**
Atamtürk's setup and production costs vary by period, and 10 of the 751 Trigeiro instances have a
production coefficient other than 1. Both are #215.

**MIRPLIB is mis-filed, on #183 and on #177.** MIRPLIB (<https://mirplib.scl.gatech.edu/>) is the
*Maritime Inventory Routing* library: vessels routed between production and consumption ports,
each port carrying an inventory balance with a storage capacity. Its solution is a set of vessel
routes *and* a delivery schedule — two structures joined by an outer loop, which is exactly
principle 2's user-level composition of `RoutingModel` and `LotSizingModel`, and the preamble
already names it as R22's (#125) relative. It is not a lot-sizing instance format, no plugin for
#183 can read it, and no lot-sizing benchmark should cite it. Both issues now carry the correction
and Trigeiro as the replacement, with the live URL.

**The nine `absent` classes.**

| class | ruling | reason |
|---|---|---|
| initial inventory | `extend` — #214 | Two coefficients on a row that already exists (period-0 balance RHS, and a lower bound on the last period's inventory column). It is what makes rolling-horizon re-planning expressible, which is the ordinary industrial use and needs no new concept. Weakly evidenced by the data — see above — and strongly by use. Covers the required *ending* inventory in the same issue, because `L0.PRN` and `LT.PRN` are the two ends of one balance |
| backlog cost (P6, #147) | `extend` — #212 | A named variant, and the engine already computes `total_backlog()` with nothing to charge it. One column family for HiGHS (`s_pt` splits into `s_pt - b_pt`), and `Result` gains `backlog[p][t]`. Neither benchmark set has it, so it is driven by P6 rather than by the data — but the sibling in the same issue, **overtime**, is in every one of the 751 Trigeiro files, and is the same shape: a priced violation column |
| setup carry-over (P5, #146) | `extend` — #213 | The strongest of the four. The Trigeiro set *is* CLSPL data — Sürie's format specification is titled for lot sizing with linked lot sizes, carry-over is a modelling choice rather than an instance field, and the same 751 files serve both problems. The day the `w_pt` link variable exists, #183 gets CLSPL coverage at no data cost and can cross-check against the published bounds |
| per-period cost variation | `extend` — #215 | Not merely nice: it is what stops Atamtürk's set being loadable. Collapsing 90 per-period setup costs into one number is a different instance, not a lossy import. Free for HiGHS, which writes a `col_cost_` entry per `(p, t)` already. #215 carries the unit production *time* with it — the same "indexed by product alone" defect, and the thing 10 of the 751 Trigeiro instances need |
| multiple resources / parallel machines (P7, #148) | `cut` | **Ruled on scope, as a single-model schema extension — principle 2 does not reach it.** A resource index on `capacity`, `setup_time` and production is one model with a wider schema, exactly like location-routing or transshipment, so the question is whether it is in scope, not whether it composes. It is not: `M = 1` in all 751 Trigeiro instances and Atamtürk has no resource dimension, so nothing COSO would benchmark needs it; and #148 as written is not the schema widening at all but "assign production to machines **and sequence**", which drags in GLSP's cut below and a scheduling engine #173's map does not offer for this model. The cut is reversible and cheap if a multi-resource set is ever curated — `capacity(resource, period)` plus `setup_time(product, resource)` is what the `.PRN` format already stores — which is the reason to cut it now rather than build it on no instances |
| minimum and maximum lot size | `cut` | Neither set has the field; in both, the only bound on a lot is the big-M capacity link. No named variant issue asks for it. Reversible in two column bounds for HiGHS, and a minimum lot size is a genuinely harder native change — it makes the empty lot and the minimum lot the only options, which the shift and split operators cannot express. Cut until an instance needs it |
| lead times | `cut` | Neither set has the field. The `LT.PRN` that looks like one is a required *ending* inventory, which #214 takes. A lead time only becomes meaningful once dependent demand works at all, so it is behind #210 as well as behind a benchmark |
| DLSP (P3, #144) | `cut` | All-or-nothing production, `x_t in {0, C_t}`, single item, small buckets. Neither set has it, #144 is closed, and it is a change of feasible *set* rather than a field: HiGHS gets it by substituting `C_t y_t` for `x_t`, which is a formulation, and the native operators — shift, merge and split arbitrary quantities — have nothing left to say when a lot may only be empty or full. There is no structure-aware oracle in #173's map for it |
| GLSP (P4, #145) | `cut` | **Ruled on scope, as a single-model schema extension — principle 2 does not reach it.** GLSP is one model: lot sizes and a sequence within each bucket, decided together. So the ruling is on scope, and it is out on three counts. The solution object changes — `Result` would carry an order and start times inside each period, which is `ScheduleModel`'s object, and §Result's derivation of setups from `production()` stops holding. Sequence-dependent setup costs and times have nowhere in the schema to go. And the native engine reasons only about period totals, so there is no structure-aware operator to exploit; Fleischmann & Meyr's instances are not in #177 either. Cut, and the N-period bucket must not be sold as a schedule |

**README.** Two rows overstated what exists and are corrected in the same commit. The model row
read `LotSizingModel | CLSP, MLCLSP`; MLCLSP is not expressible while #210 stands, so it is
narrowed to `CLSP (capacitated lot sizing)`. The engine-status row read *"Functional —
fix-and-optimize bridge"*; **there is no fix-and-optimize anywhere in the tree** — the string
occurs in `README.md` and `CLAUDE.md` and nowhere else, and the engine is lot-for-lot /
Silver-Meal / part-period balancing followed by a shift/merge/split descent. Both files now say
so, and say that a capacitated instance needing a pre-build comes back infeasible (#211).

### Defects

| issue | verdict |
|---|---|
| #210 `add_bom` is accepted and never read, so MLCLSP solves as CLSP | filed by this audit. It is the one `drops` in the native column, and **dead** rather than dormant: there is no idle dependent-demand code to wire. The interim step is the disable-and-raise of #193 — `solve()` throws while `bom_` is non-empty — and #183 makes it real, at the cost of one term in a balance row. Must be zero before #183 closes |
| #211 constructions ignore capacity and the descent cannot repair infeasibility | filed by this audit. It is why the `supported` evidence for capacity is capacity *blocking* a merge rather than capacity *forcing* a pre-build: a one-product instance with demand `(0, 20)` and capacity `(20, 10)` comes back `feasible() == false` although the plan that pre-builds in period 0 is feasible and optimal. On instances that do come back feasible it costs quality — the same 2 × 3 instance at capacity 45 returns 330 against an enumerated optimum of 290 |
| #212 backlog is an infeasibility with no cost, and capacity has no overtime | filed by this audit as the P6 `extend`; lands in #183 |
| #213 no setup carry-over between periods | filed by this audit as the P5 `extend`; lands in #183, where it unlocks CLSPL over the same 751 instances |
| #214 no initial or required ending inventory | filed by this audit as the `extend`; lands in #183 |
| #215 costs are per product only and production time is fixed at 1 | filed by this audit as the `extend` that **blocks #177 curating either benchmark set**; lands in #183 |

Two findings with no issue of their own:

- **`Result` returns no cost breakdown.** `LotsizingSolution` maintains `setup_cost()`,
  `holding_cost()` and `production_cost()` separately and only their sum reaches the caller, so the
  test recomputes the three components from `production()` and the declaration instead. It is a
  `Result` contract question for every model, so it is noted on #176 rather than filed here.
- **The existing model-level coverage was not evidence.** Before this audit,
  `tests/lotsizing/lotsizing_model_test.cpp` asserted the *shape* of `production()` and
  `inventory()` plus `cost() > 0.0` — the pattern #199's evidence rule exists to exclude, since it
  passes whatever the engine does with the declaration. One case was named "LotSizingModel supports
  BOM" and asserted two `size()` calls; it is renamed to "accepts a BOM declaration", which is what
  it shows, and the claim it made now lives in the `SKIP`-ed case that names #210. The `[lotsizing]`
  cases in `tests/model/model_test.cpp` are shape assertions too, and the `e2e_smoke` scenario is
  one product over three periods with a capacity of 80 against a peak demand of 20 — nothing there
  binds. Every `supported` cell above cites the new capacity test instead.

## Routing

`RoutingModel` (`src/model/routing_model.h`) declares depots, vehicle types with an
N-dimensional capacity and a cost structure, clients with demand, pickup, a time window and a
service duration, per-profile distance and duration matrices, pickup-delivery requests and a
reference solution, and solves it as a rich VRP: a nearest-neighbour or Clarke-Wright
construction, then `PortfolioSolver` — ILS with a first-improvement granular descent, then an
HGS-style GA, then a zero-penalty finalizer. It is the widest schema of the six and the first
milestone's model. Five slots are deleted as of this section — see §Rulings.

Engine columns are #173's map for this model: **native** and **PyVRP** (#178). Only the native
engine is integrated; PyVRP is not a dependency of this repo, so its column can hold no value
but `documented` or `—`, pinned to a public API reference:

- **PyVRP v0.14.0**, uploaded 2026-08-20, read from the type stubs shipped inside the wheel
  (`pyvrp-0.14.0-cp312-cp312-manylinux_2_27_x86_64.manylinux_2_28_x86_64.whl`,
  `pyvrp/_pyvrp.pyi`) rather than from a documentation page, so every cell below names an
  attribute that exists in the binary: `Location`, `Client`, `ClientGroup`, `Depot`,
  `Shipment` / `ShipmentStep`, `VehicleType`, `ProblemData`, `Route`, `ScheduledActivity`,
  `Solution`, plus the `pyvrp.Model` builder (`add_location`, `add_client`, `add_shipment`,
  `add_client_group`, `add_depot`, `add_edge`, `add_profile`, `add_vehicle_type`, `solve`).
  Every PyVRP cell below carries the attribute or call inline; a `—` cell says what is missing.

**One correction to #200's own PyVRP list, made before any row is filled: 0.14.0 *has* paired
pickup-delivery.** `Shipment(pickup_location, delivery_location, pickup_tw_early,
pickup_tw_late, pickup_service_duration, delivery_tw_early, delivery_tw_late,
delivery_service_duration, amount, prize, required)` is a first-class entity carried in
`ProblemData(…, shipments=[…])` and built by `Model.add_shipment`; `Route` reports
`num_shipments()` and `Activity::is_pickup()` / `is_delivery()`, and `Solution` reports
`num_missing_shipments()`. So R15 (PDPTW) is a PyVRP feature, not a gap, and #178's reject list
is one item shorter than the issue assumed. The four things 0.14.0 genuinely does not have are
**skills**, **synchronised visits**, **driver breaks** and a **third arc-cost matrix**: no
attribute in `_pyvrp.pyi` matches any of them, and the only occurrence of the string `break` in
the stub file is `PiecewiseLinearFunction.breakpoints`.

**Reachability from `solve()`, re-run at `26046c6`.** A declaration counts only if code that consumes it is reachable from `solve()`. Reachability is
derived from includers, not assumed. #200's second loop matched **basenames**, so
`src/routing/overconstrained.h` was credited with an includer it does not have —
`src/assignment/overconstrained.cpp:1` includes `assignment/overconstrained.h`. Both loops below
match the **path** an includer would have to write:

```sh
# resources/ and operators/ headers with no includer outside their own directory
for h in src/routing/resources/*.h src/routing/operators/*.h; do
  inc=$(grep -rl "\"${h#src/}\"" src/ --include='*.h' --include='*.cpp' \
        | grep -v "^$(dirname "$h")/")
  [ -z "$inc" ] && echo "idle: $h"
done
# top-level engine/search headers with no includer other than their own .h/.cpp
for h in src/routing/overconstrained.h src/search/warm_start.h src/search/partitioned_search.h \
         src/search/daemon.h src/search/guided_local_search.h; do
  inc=$(grep -rl "\"${h#src/}\"" src/ --include='*.h' --include='*.cpp' \
        | grep -v "^${h%.h}\.\(h\|cpp\)$")
  [ -z "$inc" ] && echo "idle: $h"
done
```

The first loop prints **thirteen**, exactly as #200 said: nine resources — `break_resource`,
`compartment_resource`, `depot_resource`, `loading_resource`, `precedence_resource`,
`skill_filter`, `sync_resource`, `task_count_resource`, `type_incompatibility` — and four
operators — `insert_optional`, `pair_operators`, `relocate_with_depot`, `route_split`.

The second loop prints **four** as #200's own script wrote it, and **five** once the match is
path-qualified as above. The four are `warm_start.h`, `partitioned_search.h`, `daemon.h` and
`guided_local_search.h`; the fifth is `src/routing/overconstrained.h`, which is idle in fact and
was only ever masked by the basename collision. "All five top-level headers" was therefore the
right count reached by the wrong route, and the corrected script is what this section records.

What that leaves reachable, checked by hand rather than read off a word count:

- `LocalSearch::run` (`src/routing/local_search.cpp:18-22`) instantiates `Exchange10`,
  `Exchange11`, `Exchange20`, `SwapTails` and `SwapStar`, and nothing else.
- `Route` includes the load, distance and duration resources only (`src/routing/route.h:4-6`).
- `Route::depot_` is assigned 0 in the constructor (`route.cpp:14`) and nowhere else;
  `src/routing/construction.cpp:37,98` hardcodes `depot = 0` too.
- `ProblemData::cost(profile, from, to)` — the third matrix — has no caller in `src/`.
- `CostEvaluator::set_distance_cost_function` / `set_duration_cost_function` are called only
  from `tests/routing/piecewise_cost_test.cpp`, so `piecewise_cost.{h,cpp}` is compiled into the
  model path and never installed on it.
- `ProblemData::requests()` has two callers, `src/routing/operators/pair_operators.cpp` and
  `src/routing/resources/precedence_resource.h` — both idle.

#200's per-slot word-count script is **not** used here, and should not be: it counts
comment-only hits (`quantity`'s single hit is `load_resource.h:28`, a comment about
`LoadResource::pickup` — a comment hit *and* a name collision), it collapses `tw` and `skills`
across two structs each, `cost` matches 38 files, and its `$IDLE` filter holds only `.h` paths
so idle `.cpp` files are never removed — `required` matches `operators/insert_optional.cpp` and
`search/warm_start.cpp`, `cost` matches `daemon.cpp`, `partitioned_search.cpp` and
`guided_local_search.cpp`. Every per-slot verdict in §Features was checked against the grep's
`file:line` output instead.

### Schema

| entity | fields |
|---|---|
| depot | coordinate `(x, y)` **or** an explicit node id; `tw` |
| vehicle type | `count`; `capacity` (one int per load dimension), `max_duration`, `max_distance`, `min_tasks`, `max_tasks`, `max_overtime`, `unit_overtime_cost`, `reload_depot`, `max_reloads`, `cost` (a `CostParams`), `profile`, `skills` |
| `CostParams` | `fixed_cost`, `unit_distance_cost` (default 1), `unit_duration_cost` |
| client | coordinate `(x, y)` **or** an explicit node id; `demand` and `pickup` (one int per load dimension), `tw`, `extra_tw`, `service`, `release_time`, `prize`, `required`, `group`, `skills`, `client_type` |
| request | an ordered `(pickup, delivery)` client pair |
| matrices | `set_distance` / `set_duration` on the current profile, `set_profile_distance` / `set_profile_duration` / `set_cost_matrix` on a named one |
| reference solution | `set_initial_routes(routes)`, `pin(client_id)` |

Convention, and the count this audit is against: a slot is `Struct::field`. At `26046c6`, where
the audit started, `ClientParams` had 14 + `VehicleTypeParams` 13 (`cost` counted — it is a
declarable member) + `CostParams` 4 + `DepotParams` 1 = **32 slots**, and §Features has a row
for each. Five of them — `quantity`, `setup_time`, `location`, `speed_factor` and
`per_task_hour_cost` — are deleted by §Rulings, so the table above is 27 and those five rows
read `absent`. `ClientParams::tw` and `DepotParams::tw` are two slots, and so are
`ClientParams::skills` and `VehicleTypeParams::skills`; each pair gets its own row below.

Node numbering is the one thing a caller must get right and the API never states: `set_distance`
and every matrix setter take **full node indices** — depots `0..D-1`, then clients
`D..D+C-1` — while `add_client` returns a **client index** `0..C-1` and `Result::routes()`
gives those back. The offset is the caller's to apply.

`add_pickup` and `add_delivery` are aliases of `add_client` with no added semantics
(`routing_model.cpp:52-58`); `add_pickup_delivery` is an alias of `add_request`. Nothing in the
model validates: no setter throws, an out-of-range client id in `add_request` or `pin` is stored
unchecked, and `solve()` returns a default-constructed `Result` (`feasible() == false`) when
there is no depot or no vehicle type rather than raising.

Absent, and named because the rulings below turn on them: synchronised visits between vehicles,
team formation, a route that does not return to a depot, per-client vehicle-type admissibility,
a battery or recharge resource, time-dependent travel, a client served by more than one route,
a multi-period visit pattern, and driver breaks.

### Features

Every native `supported` cell is evidenced on `tests/routing/routing_model_test.cpp`, and each
of those tests was verified by mutation — the mutation and what it broke are recorded with the
cell. The instance the capacity and pickup cells share is one depot at the origin and three
clients on a line at (10, 0), (20, 0) and (30, 0): the single tour costs 60 and three singleton
routes cost 120, so distance alone always prefers one route and only a declared capacity can
split it.

| slot | model | native | PyVRP 0.14.0 |
|---|---|---|---|
| `ClientParams::demand` | `declarable` | `supported` [a] | `documented` — `Client.delivery: list[int]` |
| `ClientParams::pickup` | `declarable` | `supported` [b] | `documented` — `Client.pickup: list[int]` |
| `ClientParams::tw` | `declarable` | `drops` [c] — **wired**, #194 | `documented` — `Client.tw_early` / `tw_late` |
| `ClientParams::extra_tw` | `declarable` | `drops` [d] — **dead** | `—` — one window pair per `Client`; no list |
| `ClientParams::service` | `declarable` | `drops` [e] — **wired**, #194 | `documented` — `Client.service_duration` |
| `ClientParams::release_time` | `declarable` | `drops` [f] — **dead** | `documented` — `Client.release_time` |
| `ClientParams::prize` | `declarable` | `drops` [g] — **dormant**, #196 | `documented` — `Client.prize` |
| `ClientParams::required` | `declarable` | `drops` [h] — **dormant**, #196 | `documented` — `Client.required` |
| `ClientParams::group` | `declarable` | `drops` [i] — **dead**, #196 | `documented` — `Client.group` + `ClientGroup(clients, required, *, name)`, with `mutually_exclusive` a read-back attribute |
| `ClientParams::quantity` | `absent` [j] | `—` [j] | `—` [j] |
| `ClientParams::skills` | `declarable` | `drops` [k] — **dormant**, #196 | `—` — no skill or qualification attribute anywhere in `_pyvrp.pyi` |
| `ClientParams::setup_time` | `absent` [l] | `—` [l] | `—` [l] |
| `ClientParams::location` | `absent` [m] | `—` [m] | `—` [m] |
| `ClientParams::client_type` | `declarable` | `drops` [n] — **dormant**, #196 | `—` — no type or incompatibility matrix |
| `VehicleTypeParams::capacity` | `declarable` | `supported` [o] | `documented` — `VehicleType.capacity: list[int]` |
| `VehicleTypeParams::max_duration` | `declarable` | `drops` [p] — **dormant**, #194 | `documented` — `VehicleType.shift_duration` |
| `VehicleTypeParams::max_distance` | `declarable` | `drops` [q] — **dormant**, #194 | `documented` — `VehicleType.max_distance` |
| `VehicleTypeParams::min_tasks` | `declarable` | `drops` [r] — **dormant**, #196 | `—` — no minimum visit count per vehicle |
| `VehicleTypeParams::max_tasks` | `declarable` | `drops` [s] — **dormant**, #196 | `—` — no maximum visit count per vehicle |
| `VehicleTypeParams::max_overtime` | `declarable` | `drops` [t] — **dormant**, #196 | `documented` — `VehicleType.max_overtime` |
| `VehicleTypeParams::unit_overtime_cost` | `declarable` | `drops` [u] — **dead**, #196 | `documented` — `VehicleType.unit_overtime_cost` |
| `VehicleTypeParams::reload_depot` | `declarable` | `drops` [v] — **dormant**, #196 | `documented` — `VehicleType.reload_depots: list[int]` |
| `VehicleTypeParams::max_reloads` | `declarable` | `drops` [w] — **dormant**, #196 | `documented` — `VehicleType.max_reloads` |
| `VehicleTypeParams::cost` | `declarable` | `drops` [x] — **wired**, #198 | `documented` — the three cost attributes below |
| `VehicleTypeParams::profile` | `declarable` | `drops` [y] — **wired**, #198 | `documented` — `VehicleType.profile`, `ProblemData.distance_matrices` |
| `VehicleTypeParams::speed_factor` | `absent` [z] | `—` [z] | `—` [z] |
| `VehicleTypeParams::skills` | `declarable` | `drops` [aa] — **dormant**, #196 | `—` — no skill attribute; see `ClientParams::skills` |
| `CostParams::fixed_cost` | `declarable` | `drops` [ab] — **wired**, #198 | `documented` — `VehicleType.fixed_cost` |
| `CostParams::unit_distance_cost` | `declarable` | `drops` [ac] — **wired**, #198 | `documented` — `VehicleType.unit_distance_cost` |
| `CostParams::unit_duration_cost` | `declarable` | `drops` [ad] — **wired**, #198 and #221 | `documented` — `VehicleType.unit_duration_cost` |
| `CostParams::per_task_hour_cost` | `absent` [ae] | `—` [ae] | `—` [ae] |
| `DepotParams::tw` | `declarable` | `drops` [af] — **wired**, #194 | `documented` — `Depot.tw_early` / `tw_late` |

The structural features, which are methods rather than slots:

| feature | model | native | PyVRP 0.14.0 |
|---|---|---|---|
| more than one depot (`add_depot` ×N) | `declarable` | `drops` [ag] — **dormant**, #196 | `documented` — `VehicleType.start_depot` / `end_depot` per type |
| paired pickup-delivery (`add_request`) | `declarable` | `drops` [ah] — **dormant**, #196 | `documented` — `Shipment`, `Model.add_shipment` |
| client groups (`add_client_group`) | `declarable` | `drops` [ai] — **dead**, #196 | `documented` — `ClientGroup`, `Model.add_client_group` |
| third arc-cost matrix (`set_cost_matrix`) | `declarable` | `drops` [aj] — **dead**, #196 | `—` — distance and duration matrices only; cost is `unit_distance_cost * distance + unit_duration_cost * duration` |
| reference solution (`set_initial_routes`, `pin`) | `declarable` | `drops` [ak] — **dormant**, #193 | `—` as a *declaration*: `solve(…, initial_solution=Solution)` is a hint on the call, and there is no pinning at all. See §Rulings |
| explicit node ids (`add_client(int id, …)`) | `declarable` | `drops` [al] — **wired wrong**, #220 | `documented` — `Client.location` indexes the matrices directly |

Evidence:

- [a] `tests/routing/routing_model_test.cpp` "RoutingModel: vehicle capacity splits a route the
  demand overfills". Three clients of `demand = {6}` against `capacity = {10}` come back as
  three singleton routes — two clients are 12 units against a declared 10, so *every* feasible
  route set is exactly that — and the test recomputes each returned route's delivery load from
  the declared demand and asserts it is within the declared capacity. The control section is
  the same declaration with `capacity = {20}` and returns **one** route holding all three,
  which is what an engine that ignored demand or capacity would return in both. Enforced by
  `LoadResource::init` (`load_resource.h:52`) and `LoadResource::excess`
  (`load_resource.h:136-146`) through `Route::update`, `CostEvaluator::route_penalty` and
  `Solution::feasible()`. Verified by mutation: replacing
  `.demand = std::move(p.demand)` with `.demand = {}` in `ProblemData::Builder::add_client`
  makes the capacitated section return one route and the test fails 4 assertions.
  Two further sections carry the N-dimensional case, which the granularity rule makes part of
  the same feature: `capacity = {100, 10}` against `demand = {1, 6}` is slack in dimension 0 at
  3 units against 100 and binding in dimension 1 at 18 against 10, and returns three routes,
  while the control at `{100, 100}` returns one. Enforced by the `d`-loop in
  `LoadResource::excess`; restricting that loop to `d == 0` fails those sections at 2
  assertions while leaving the one-dimensional sections green.
- [b] `tests/routing/routing_model_test.cpp` "RoutingModel: backhaul pickup loads the vehicle
  like demand does". The same three clients with `demand = {0}` and `pickup = {6}` split into
  three routes, and the control with `pickup = {0}` returns one — delivery demand is zero
  throughout, so nothing but `pickup` can cause the split. Enforced by `load_resource.h:53-54`
  and by the merge rule `max(l.load + r.delivery, r.load + l.pickup)` at
  `load_resource.h:103-105`, which is what makes an accumulating backhaul peak at the end of the
  route rather than the start. Verified by mutation: `.pickup = {}` in the builder collapses it
  to one route and the test fails at the `routes().size() == 3` and per-route load assertions.
- [c] `DurationResource::init` reads `c.tw.start` and `c.tw.end`
  (`src/routing/resources/duration_resource.h:51-52`), `Route::update` maintains `time_warp_`
  from it, and `CostEvaluator::route_penalty` charges `time_warp * tw_penalty_`
  (`cost_evaluator.cpp:93`) — so the search does push toward the declared windows. What is
  missing is the classification: `Solution::feasible()` (`solution.cpp:78-85`) checks
  `load_feasible()` on every route and nothing else, so a route that violates every declared
  window is returned with `feasible() == true`. **#194.** The assertion that should hold is in
  the `SKIP`-ed "RoutingModel: client time windows are respected by the returned route", which
  recomputes arrival times from the returned route and the declared travel times. Two of the
  five operators make it worse: `Exchange11` and `Exchange20` build their own deltas with no
  time-warp term at all (#221).
- [d] Zero consumers. `extra_tw` is copied into `ProblemData::ClientData`
  (`problem_data.cpp:27`) and the string does not occur again anywhere in `src/routing` or
  `src/search`. **Dead**: there is no multi-window code to wire, idle or otherwise.
- [e] `service` reaches `DurationResource::init` (`duration_resource.h:51,53`, as
  `earliest = tw.start + service` and as the segment's `duration`) and
  `DistanceResource::init` (`distance_resource.h:46`), both reachable. But its only
  route-*observable* effect runs through arrival times, and those are exactly what #194 leaves
  unenforced — with no window that can be violated, no returned route changes when the declared
  service time does. The `SKIP`-ed "RoutingModel: service time delays the arrivals after it"
  holds the assertion, naming #194. So the cell is `drops` for the same reason `tw` is, not for
  want of code.
- [f] Zero consumers, same test as [d]. `release_time` is stored and never read. **Dead.**
- [g] `prize` *is* read on the model path — `CostEvaluator::route_objective` subtracts it per
  served client (`cost_evaluator.cpp:80`) and the insert/remove deltas carry it
  (`cost_evaluator.cpp:139,176`). It changes nothing, because the served set never changes:
  `construction.cpp` has no `required` check and serves every client, and `crossover.cpp` step 5
  reinserts every missing one, so the prize total is a constant offset on the objective.
  **Dormant** — `src/routing/operators/insert_optional.{h,cpp}`, the only code that would
  remove an unprofitable client, is idle. #196.
- [h] `required` has no consumer in the reachable set at all; its two consumers are
  `src/routing/overconstrained.cpp:12` and `src/routing/operators/insert_optional.cpp:25,71` (line 65 of the former is a comment), plus a third in `src/search/warm_start.cpp:109`, all
  idle. `required = false` therefore has no effect. **Dormant.** #196.
- [i] `ClientParams::group` has no consumer anywhere. `add_client_group()` is a counter
  (`routing_model.cpp:68-70`) and no code reads `ClientData::group`; the `group` hits in
  `src/routing/resources/sync_resource.h` are a *sync* group's own `group_id`, an unrelated
  concept in an idle file. **Dead** — there is no group operator to wire. #196.
- [j] **Deleted by this section** — §Rulings, redundant with the pickup client's `pickup[]` and
  the delivery client's `demand[]`. Before the cut it was `declarable` / `drops` / **dead**:
  `quantity`'s only occurrence in `src/routing` outside the copy was the comment at
  `load_resource.h:28` describing `LoadResource::pickup`. Neither engine has a row to hold now
  that the API cannot make the declaration.
- [k] `ClientParams::skills` reaches `src/routing/resources/skill_filter.h` and nothing else;
  that header has no includer outside `src/routing/resources/`. **Dormant.** #196.
- [l] **Deleted by this section** — §Rulings, redundant with `service`. Before the cut it was
  `declarable` / `drops` / **dead**: zero consumers anywhere in `src/routing` or `src/search`.
- [m] **Deleted by this section** — §Rulings, redundant with the client's node id. Before the cut
  it was `declarable` / `drops` / **dead**: zero consumers.
- [n] `client_type` reaches `src/routing/resources/type_incompatibility.h:106` and nothing
  else; that header is idle. **Dormant.** #196.
- [o] The same test as [a]: capacity is the half of the pair the control section isolates, since
  the two sections differ in `capacity` alone — `{10}` against `{20}`, and `{100, 10}` against
  `{100, 100}` — and return three routes against one. Verified by mutation independently of [a]'s: making `LoadResource::excess` return
  0 unconditionally leaves capacity unenforced and fails both the capacity and the pickup tests,
  6 assertions in all.
- [p] `max_duration` is read twice — `DistanceResource::excess` (`distance_resource.h:103-104`)
  and `DurationResource::excess` (`duration_resource.h:154-155`) — and both feed
  `Route::dist_excess_` (`route.cpp:291`). Nothing consumes it: `CostEvaluator::route_penalty`
  adds load excess and time warp only, `dist_penalty_` is stored and never applied, and
  `Solution::feasible()` does not look. `dist_excess()` and `dist_feasible()` have no caller in
  `src/` at all. **Dormant**, and it is #194's second half rather than a separate bug.
- [q] `max_distance` — the same mechanism, `distance_resource.h:99-100`. **Dormant.** #194.
- [r] `min_tasks` reaches `src/routing/resources/task_count_resource.h:59` only; the header is
  idle. **Dormant.** #196.
- [s] `max_tasks` — same header, `task_count_resource.h:64`. **Dormant.** #196.
- [t] `max_overtime` reaches `PiecewiseLinearFunction::overtime`
  (`src/routing/piecewise_cost.cpp:137-148`) only. `piecewise_cost.h` is included by
  `cost_evaluator.h`, so it compiles into the model path, but nothing installs a function on
  it: `set_distance_cost_function` and `set_duration_cost_function` are called only from
  `tests/routing/piecewise_cost_test.cpp`. **Dormant** — the function exists, the wiring does
  not. #196.
- [u] Zero consumers. The overtime *tier* exists in `piecewise_cost.cpp`, but it takes an
  `overtime_rate` argument that no caller supplies and no code reads
  `VehicleTypeData::unit_overtime_cost`. **Dead.** #196.
- [v] `reload_depot` reaches `src/routing/operators/relocate_with_depot.{h,cpp}` only, which is
  idle; `LocalSearch::run` instantiates five operators and this is not one of them.
  **Dormant.** #196.
- [w] `max_reloads` — same operator, `relocate_with_depot.cpp:64`. **Dormant.** #196.
- [x] `VehicleTypeParams::cost` is the aggregate the three `CostParams` rows decompose, and it
  is read on the model path: `CostEvaluator::route_objective` takes `vt.cost`
  (`cost_evaluator.cpp:64`) and prices distance, duration and the fixed cost from it. **Wired**
  — so the cell is not `drops` for want of engine code. It is `drops` because nothing about it
  is *checkable* from what comes back: `RoutingModel::solve()` sets
  `result.cost_ = best.total_distance()` (`routing_model.cpp:196`) and `Result` carries no
  vehicle type per route, so a third party holding the returned routes cannot recompute the
  quantity the search minimised. **#198.** Evidenced instead on the model axis — see the
  round-trip note below the table.
- [y] `profile` selects the matrix everywhere it matters — `Route::update` and the four
  incremental evaluators take `data_->vehicle_type(vehicle_type_).profile`
  (`route.cpp:77,99,124,147,165,180,227`), `construction.cpp:36,106-108` uses it, and the
  exchange operators re-read it per move. **Wired.** `drops` for the same reason as [x]: a
  profile is a property of the *vehicle type*, `Result::routes()` is a list of client ids with
  no vehicle type attached, and `cost_` is total distance, so nothing in a returned solution
  says which matrix priced it. #198, and see the `Result` list in §Result.
- [z] **Deleted by this section** — §Rulings, redundant with `profile`. Before the cut it was
  `declarable` / `drops` / **dead**: zero consumers. PyVRP has no per-type scaling either — one
  duration matrix per profile.
- [aa] `VehicleTypeParams::skills` reaches `skill_filter.h` only, the same idle header as [k].
  **Dormant.** #196.
- [ab] `fixed_cost` is charged per non-empty route in `CostEvaluator::route_objective`
  (`cost_evaluator.cpp:75`) and carried in the insert, remove, pair and replace deltas
  (`cost_evaluator.cpp:135,172`; `exchange.cpp:68,110`). **Wired**, `drops` per [x] — a declared
  `fixed_cost = 1000` never reaches `Result::cost()`. The `SKIP`-ed "RoutingModel: Result::cost
  is the value of the declared objective" holds the assertion that should pass, naming #198.
- [ac] `unit_distance_cost` multiplies every distance term in the objective and in all four
  delta paths (`cost_evaluator.cpp:44,119,167`; `exchange.cpp:66,108,146`). **Wired**, `drops`
  per [x].
- [ad] `unit_duration_cost` is priced in `CostEvaluator::route_objective` only
  (`cost_evaluator.cpp:51,72`). **No move delta prices it** — `eval_insert_cost` /
  `eval_remove_cost` skip the duration term by design (`cost_evaluator.cpp:122-131`, whose
  comment justifies it by the field's default of 0), and the pair and replace deltas in
  `exchange.cpp` have no duration term either. So with `unit_duration_cost > 0` declared, the
  descent accepts moves on a function the evaluator does not agree with. Filed as **#221**;
  `drops` per [x] on top of that.
- [ae] **Deleted by this section** — §Rulings, redundant with `unit_duration_cost`. Before the
  cut it was `declarable` / `drops` / **dead**: zero consumers, and nothing in the schema defined
  what a "task hour" was.
- [af] `DepotParams::tw` reaches `DurationResource::init_depot`
  (`duration_resource.h:67-68`), which is where a route's earliest departure and latest return
  come from, so the depot window does enter the time-warp penalty. It is unenforced for exactly
  the reason [c] is — `Solution::feasible()` is load-only. **#194.** The `SKIP`-ed
  "RoutingModel: the depot time window bounds the route" holds the assertion.
- [ag] `add_depot` accepts any number of depots and `ProblemData` stores them all, but every
  route starts and ends at node 0: `Route::depot_` is assigned 0 in `Route::Route`
  (`route.cpp:14`) and never again, and `construction.cpp:37,98` hardcodes `depot = 0`.
  `src/routing/resources/depot_resource.h` — the code that would assign a depot per route — is
  idle. **Dormant.** #196.
- [ah] `add_request` stores the pair and `ProblemData::requests()` returns it, but its only two
  consumers are `src/routing/operators/pair_operators.cpp` and
  `src/routing/resources/precedence_resource.h`, both idle. Construction and crossover treat the
  two clients as unrelated, so a returned solution routinely puts a pickup and its delivery on
  different vehicles or the delivery first. **Dormant.** #196.
- [ai] `add_client_group()` returns `next_group_id_++` and stores nothing
  (`routing_model.cpp:68-70`); with `ClientParams::group` dead per [i], the whole feature is a
  counter. **Dead.** #196.
- [aj] `set_cost_matrix` fills `ProblemData::cost_matrices_`, and `ProblemData::cost(profile,
  from, to)` has no caller in `src/`. **Dead** — the matrix is built on every `build()` and read
  by nobody. #196.
- [ak] `set_initial_routes` and `pin` write `initial_routes_` and `pinned_`, and `solve()` reads
  neither (`routing_model.cpp:116-226` touches `depots_`, `clients_`, `vehicle_types_`,
  `requests_` and the three matrix vectors and nothing else). `src/search/warm_start.h`
  implements `warm_start()`, `PinSet`, `replan()` and `local_search_with_pins()` and is idle.
  A user who pins a client and re-solves gets a solution that ignores the pin, reported
  feasible. **Dormant.** **#193.**
- [al] The explicit-id overloads store `explicit_id` and `has_coord = false`, and `solve()`
  ignores both: it builds `Coord{d.x, d.y}` for every depot and `Coord{c.x, c.y}` for every
  client (`routing_model.cpp:133,139`), so an explicitly-numbered node is at (0, 0). The
  builder then fills all three matrices with Euclidean distances from those coordinates and the
  caller's `set_distance` calls override only the pairs they name, leaving every unnamed pair at
  0 — and the granular k-NN lists are sorted on that matrix (`problem_data.cpp:209`). Not
  `drops` in the ordinary sense: the declaration is read, and read wrongly. Filed as **#220**.

**The objective slots are evidenced on the model axis, not on a returned route.** For `cost`,
`fixed_cost`, `unit_distance_cost`, `unit_duration_cost` and `profile` no route set is
*infeasible* — they are objective parameters, and the evidence rule's "only route sets that
respect the declaration are feasible" has nothing to bite on. Their model-axis `declarable` is
evidenced by "RoutingModel: the objective parameters round-trip and solve" in
`tests/routing/routing_model_test.cpp`: the fields are set on a `VehicleTypeParams`, read back
off the same public aggregate, passed to `add_vehicle_type`, and `solve()` returns a feasible
result over a profile-1 matrix. **This is the interim form.** `CostParams` and
`VehicleTypeParams` are public aggregates already `def_rw`-bound
(`python/bindings.cpp:53-58,106`), so the round-trip works today without #216 — but a
round-trip through the caller's own struct is weaker than a round-trip through the model, and
#216's read-back accessors on `RoutingModel` are the real form. When they land, this test should
read the values back off the model.

### Result

Primary: **the ordered client ids of each route, and the vehicle type and start/end depot of
the route that serves them.** The client sequence alone is not enough to verify a `supported`
cell the moment more than one vehicle type or more than one depot is declared — which is R3 and
R4, two of the six variants #118 claimed as core.

What a returned `Result` must carry for a third party to check a returned routing solution
against the declaration, feature by feature:

| needed for | field |
|---|---|
| any per-type feature — capacity, `max_distance`, the cost structure, `profile` (R3 HFVRP) | the **vehicle type** of each route |
| multi-depot (R4), open VRP (R5) | the **start and end depot** of each route |
| multi-trip (R12) | the **trip boundaries**: where the route returns to a reload depot, and which one |
| time windows and service (R2 VRPTW, R27 TRSP) | **arrival, start-of-service, departure and wait** per visit, and the route's start and end time |
| optional clients and groups (R8, R11) | the **unserved set** (present today) and, per group, which member was served |
| the objective | `cost` = **the value of the declared objective**, plus the breakdown that prices it: fixed cost, distance cost, duration cost, prizes collected, overtime |

That list is not invented: PyVRP 0.14.0's `Route` returns `vehicle_type()`, `start_depot()`,
`end_depot()`, `num_trips()`, `schedule()` — a list of `ScheduledActivity(trip, start_time,
end_time, duration, wait_duration, time_warp)` — plus `distance()`, `distance_cost()`,
`duration()`, `duration_cost()`, `fixed_vehicle_cost()`, `prizes()`, `overtime()`,
`excess_load()`, `excess_distance()` and `time_warp()`. Every row above is an attribute that a
comparable engine already returns, which is what makes it a contract #176 can demand rather
than a wish list.

What a returned `Result` carries today:

| field | contents |
|---|---|
| `cost()` | **`Solution::total_distance()`** — the sum of `Route::distance()` over routes, *not* the declared objective (`routing_model.cpp:196`) |
| `feasible()` | `Solution::feasible()`: every route load-feasible. Time windows, `max_distance` and `max_duration` are not consulted (#194) |
| `routes()` | one entry per non-empty route, holding that route's client ids in order. No vehicle type, no depot, no times |
| `unserved()` | `Solution::unassigned()` — always empty on the model path, since construction serves every client and crossover reinserts every missing one |
| `iterations()`, `work_ticks()` / `work_units()`, `elapsed_seconds()` | search and work counters |

Three findings:

1. **`cost()` is not the value of the declared objective.** The search minimises
   `Solution::cost(eval)` = `Solution::objective(eval)` + penalty, and `route_objective` is
   distance cost + duration cost + fixed cost − prizes (`cost_evaluator.cpp:58-84`); `solve()`
   then reports `total_distance()`. With `fixed_cost`, `unit_duration_cost` or
   `unit_distance_cost != 1` declared, the number returned is not the number minimised, and two
   engines cross-validated on it compare different quantities. **#198**, and it is what makes
   every objective row above `drops` rather than `supported`. CVRP with unit distance cost and
   no fixed cost is unaffected, which is why the Uchoa numbers are consistent.
2. **`routes()` cannot be costed.** With one vehicle type and one depot the type and the depot
   are recoverable by construction, which is why the `demand`, `pickup` and `capacity` cells can
   be evidenced at all — every test above declares exactly one of each. Declare two vehicle
   types and the returned routes stop being checkable: the same client sequence costs differently
   under each type's `cost` and `profile`, and nothing says which was used. This is #207's
   finding in the packing section, one model over: it belongs with #176's `Result` contract.
3. **`unserved()` exists and is unreachable.** `Result::unserved_` is populated from
   `Solution::unassigned()`, and nothing on the model path ever leaves a client unassigned — so
   the field that would carry R8's answer is permanently empty rather than absent. That is the
   right shape and the wrong state; it becomes real when #196's `insert_optional` is wired.

### Variants

R1–R15 are #118, closed as "core engine, all items verified complete in code"; R16–R28 are
#119–#131 under #132. The verdicts below are against the feature table, not against those
issues' checkboxes, and they do not agree with them: eight of the fifteen core variants are
declarable and dropped.

| claim | features needed | verdict |
|---|---|---|
| R1 CVRP (README, #118) | `demand`, `capacity`, distance objective | **expressible now**: [a], [o] |
| R2 VRPTW (#118) | `ClientParams::tw`, `DepotParams::tw`, `service` | declarable and **dropped** — #194. The windows are penalised, never enforced, and a violating solution is returned `feasible() == true` |
| R3 HFVRP (#118) | several vehicle types with different `capacity`, `cost`, `profile` | declarable; capacity per type is [o], but the cost structure that makes a fleet heterogeneous is `drops` (#198) and `Result` does not say which type served each route — **not expressible today** |
| R4 MDVRP (#118) | more than one depot, a start/end depot per route | declarable and **dropped** — [ag], #196. Every route starts and ends at node 0 |
| R5 Open VRP (#118) | a route that does not return to its depot | `absent` — §Rulings, `extend` #222 |
| R6 VRPSPD (#118) | `demand` and `pickup` on the same client | **expressible now**: [a] + [b], and `LoadResource`'s merge rule is exactly the simultaneous-pickup-and-delivery peak load |
| R7 VRPB (#118) | `pickup`, plus all linehauls before all backhauls on a route | the quantities are R6; the **ordering is `absent`** — §Rulings. Solvable today only as its VRPSPD relaxation, so a gap against a Goetschalckx best-known is not comparable |
| R8 Team Orienteering (#118) | `prize`, `required`, `max_duration` | declarable and **dropped** — [g], [h], [p], #196. No client is ever left unserved |
| R9 multi-dimensional capacity (#118) | `demand` and `capacity` as vectors | **expressible now**: [a]'s two N-dimensional sections |
| R10 routing profiles (#118) | `profile` per vehicle type, per-profile matrices | declarable and read ([y]) but **not verifiable**: `Result` carries no vehicle type per route and `cost()` is total distance — #198, §Result finding 2 |
| R11 client groups (#118) | `add_client_group`, `ClientParams::group` | declarable and **dropped** — [i], [ai], #196. Both halves are dead |
| R12 multi-trip (#118) | `reload_depot`, `max_reloads`, trip boundaries in `Result` | declarable and **dropped** — [v], [w], #196 |
| R13 release times (#118) | `release_time` | declarable and **dropped** — [f], #196. Dead, not dormant |
| R14 overtime (#118) | `max_overtime`, `unit_overtime_cost` | declarable and **dropped** — [t], [u], #196 |
| R15 PDPTW (#118) | `add_request`, precedence and same-route, plus R2 | declarable and **dropped** — [ah], #196, and the time-window half is #194 |
| R16 CARP (#119) | arc demands | `cut` as a model feature — §Rulings |
| R17 Cumulative CVRP (#120) | a sum-of-arrival-times objective | `absent`; `cut` — §Rulings |
| R18 Split Delivery VRP (#121) | a client served by more than one route | `absent`; `cut` — §Rulings |
| R19 Time-Dependent VRP (#122) | travel time as a function of departure time | `absent`; `cut` — §Rulings |
| R20 Electric VRP (#123) | a battery resource and recharge stations | `absent`; `cut` — §Rulings |
| R21 Period VRP (#124) | a horizon and a visit pattern per client | `absent`; `cut` — §Rulings |
| R22 Inventory Routing (#125) | routing plus an inventory balance per customer | `cut` by principle 2 — two models joined by an outer loop, not one archetype |
| R23 Location-Routing (#126) | optional depots with a fixed cost and a capacity | `absent`; `extend` #223 — §Rulings |
| R24 Site-Dependent VRP (#127) | `skills` on clients and vehicle types | declarable and **dropped** — [k], [aa], #196 |
| R25 Clustered VRP (#128) | a cluster id per client, served contiguously | `absent`; `cut` — §Rulings |
| R26 VRP with Transshipment (#129) | satellite nodes and a two-echelon solution | `absent`; `cut` — §Rulings |
| R27 TRSP (#130) | `skills`, R2's windows, **team formation** | `skills` dropped ([k], [aa]); team formation `absent` and `cut` — §Rulings. #175's "covered at schema level" is corrected |
| R28 HHCRP (#131) | `skills`, R2's windows, **synchronised visits** | `skills` dropped; sync `absent`, `extend` #224 — §Rulings |
| TSP (preamble ruling) | one vehicle, no capacity | **expressible now**: `tests/routing/routing_model_test.cpp` "RoutingModel: one uncapacitated vehicle solves a TSP" returns the unique 40-unit perimeter tour of a 10 × 10 square, where every tour using a diagonal costs 48 |
| TSPTW (preamble ruling) | TSP plus enforced windows | after #194. The `SKIP`-ed "RoutingModel: TSPTW solves as one time-feasible tour" holds the assertion |
| PC-TSP (preamble ruling) | TSP plus `prize` and `required` | after #196. The `SKIP`-ed "RoutingModel: an unprofitable optional client is left unserved" holds it |
| README "CVRP, VRPTW, PDPTW, heterogeneous fleet, multi-depot, multi-trip, …" | — | four of the six named are dropped, and the `…` claims an unbounded rest. Rewritten in the same commit from this table — §Rulings |

### Rulings

**The deletion rule is suspended, so each cut below needed a *modelling* reason** — the feature
cannot be said coherently, it duplicates another, or it belongs to a different archetype
(CLAUDE.md, **Current phase**). "Dead natively and `—` for PyVRP" is not one of them, which is
why three of #200's eight candidates are kept.

| candidate | verdict | reason |
|---|---|---|
| `ClientParams::quantity` | **delete** | *Redundant.* It is documented "for pickup/delivery requests", and a request's per-dimension quantity is already declared twice over: the pickup client's `pickup[]` and the delivery client's `demand[]`. `quantity` is a single scalar with no dimension index, so it cannot even carry what those two carry. PyVRP puts the same number in one place, `Shipment.amount`, which is the shape an `add_request` `extend` should take — not a third field on `Client` |
| `ClientParams::setup_time` | **delete** | *Redundant with `service`.* Both are a fixed time charged at a client, and nothing in the schema distinguishes them: there is no "setup" entity, and `DurationResource` would add them into the same segment duration. The form of setup time that is *not* redundant is the sequence-dependent one, `setup(i, j)`, and that is `absent` — a matrix the schema has no place for. A second name for `service` is a smaller schema, not a bigger one |
| `ClientParams::location` | **delete** | *Redundant.* A client's node id already is its location: it indexes every distance, duration and cost matrix, and it is what PyVRP calls `Client.location`. The field's own comment, "location id for location-aware setup", names the feature it existed for — the `setup_time` above — and that is going too. Two clients at the same place are declared by giving them the same explicit node id |
| `VehicleTypeParams::speed_factor` | **delete** | *Redundant with `profile`.* A per-type multiplier on travel time is a duration matrix scaled by that factor, and per-type duration matrices are exactly what `profile` and `set_profile_duration` already declare. Keeping both means two ways to say one thing and an unstated question about which wins. PyVRP made the same call: one duration matrix per profile, no per-type scaling |
| `CostParams::per_task_hour_cost` | **delete** | *Redundant with `unit_duration_cost`.* The "task hours" it would price are the clients' `service` times, and those are already inside `Route::duration()`, which `unit_duration_cost` prices. There is no task entity in the schema distinct from a client visit, and — alone among all 32 slots — the field names a physical unit, the hour, that nothing in the model defines: every other time is a dimensionless integer |
| `ClientParams::extra_tw` | **keep**, `drops`/dead | Multiple time windows is a **real and distinct VRP feature**, and this file's own granularity rule uses `time_windows` versus `multiple_time_windows` as its worked example of two features rather than one. That no engine in #173's map has it is evidence about the engines. It stays declarable and its row says it is dropped |
| `RoutingModel::set_cost_matrix()` | **keep**, `drops`/dead | A **third arc-cost matrix, independent of distance and duration, is not redundant with either** — that is the whole point of it. Distance and duration are physical; an arc *cost* is a tariff, a toll, a congestion charge or a contract rate, and it is the one of the three a user cannot derive from the other two. PyVRP prices arcs as `unit_distance_cost * distance + unit_duration_cost * duration` and so cannot express it, which is a `—` in its column and not a verdict on the schema |
| `RoutingModel::add_client_group()` | **keep**, `drops`/dead | It is the **only constructor of a group id**. Deleting it while keeping `ClientParams::group` — which #200 did not propose to cut — makes the feature unsayable: a user could set `group = 3` with nothing that says what group 3 is or which members it has. That is internally inconsistent, and inconsistency is not what the deletion rule is for. Both halves stay, both are `drops`/dead, and PyVRP's `ClientGroup(clients, required, *, name)`, with `mutually_exclusive` a read-back attribute is the shape #178 would wire them to |

The five deletions landed in one commit across `src/model/types.h`,
`src/model/routing_model.h`, `src/routing/problem_data.{h,cpp}`, `python/bindings.cpp`,
`tests/model/model_test.cpp`, `tests/routing/problem_data_test.cpp` and
`python/tests/test_routing.py`. The `ProblemData::ClientData` and `VehicleTypeData` mirrors went
with them: unlike #195's network resources, no engine-layer test reads any of the five, so
leaving the fields behind would leave members that nothing writes and nothing reads.
`CostParams` drops to three fields, which is the only one of the five visible in a Python
`repr` — `CostParams(fixed=…, dist=…, dur=…)`. Two existing tests changed rather than being
deleted: `tests/model/model_test.cpp` "RoutingModel pickup-delivery workflow" declared
`{.quantity = 3}` on both ends of a request and now declares `{.pickup = {3}}` on the pickup and
`{.demand = {3}}` on the delivery, which is what the ruling says that number always was; and the
`speed_factor` line of `tests/routing/problem_data_test.cpp` "ProblemData vehicle type data
preserves attributes" is gone, with `profile` still asserted beside it. The class comment on
`RoutingModel` claimed VRPTW, multi-trip, pickup-delivery, optional clients and client groups;
it now says CVRP is what the native engine enforces and points here.

**Principle 4 applied to `set_initial_routes()` and `pin()` — the API splits in two, and the
split is #176's to make.** `pin(client_id)` is defined against the routes given to
`set_initial_routes()`: pinning a client means keeping it where that solution put it. So those
routes are not a hint. They are part of the problem — a **reference solution**, which an engine
must honour or reject, and which #176 must be able to introspect. The same call is also the
only warm start the API has, and a warm start with no pins is a pure **hint**: an engine may
ignore it and still be correct.

The ruling: `set_initial_routes()` + `pin()` stay in the model as one declaration, the
reference solution; the hint-only role moves to the `solve()` call. **The API change is #176's
and is not made here** — this section records the split and the reason. Two pieces of evidence
that it is the right cut: `AssignmentModel::set_published_schedule()` + `set_change_penalty()`
is already the declaration form of the same idea, and PyVRP puts the hint exactly where this
ruling puts it — `Model.solve(stop, …, initial_solution: Solution | None = None)` is a
parameter of the call, not of `ProblemData`, and PyVRP has no pinning at all. `src/search/
daemon.h` stays engine-only for the same reason: a model is a declaration, and re-solving
against a reference solution is the protocol, not a mode.

**Synchronised visits — `extend`, filed as #224.** R28 (HHCRP) and half of R27 are about two
vehicles visiting the same client within a tolerance of each other, and there is no way to say
it: `RoutingModel` has no cross-route constraint of any kind. The engine has the resource —
`src/routing/resources/sync_resource.h` defines a `SyncGroup` with a group id, its member
clients and a time tolerance — and it is idle with no model-side declaration to fill it, which
is #175's correction. The sketch, additive per principle 5:

```cpp
/// A set of clients that must be visited within `tolerance` time units of
/// each other, by different vehicles.  Returns the sync group id.
int add_sync_group(std::vector<int> const& clients, int tolerance);
```

A model with no `add_sync_group` call is exactly today's model. Owner: #131 (R28), with #130
(R27) as the second consumer; #178 must reject it, since PyVRP 0.14.0 has no synchronisation.
It is an `extend` rather than a `cut` because it is the one feature in this section that COSO's
own engine already has code for and a funded product category asks for.

**Team formation — `cut`.** R27's other half is technicians grouped into teams for a job, where
the *team* is a decision: which technicians ride together, and therefore what the combined
skill set of the vehicle is. That is not a wider routing schema; it is a set-partitioning
decision over technicians feeding a routing decision, which is principle 1 (a formulation, not
a structure) sitting on top of principle 2 (two structures joined by an outer loop). Cut, and
R27 is expressible as TRSP with **fixed** teams — each team declared as one vehicle type
carrying that team's skills — once `skills` is wired. Say so on #130 rather than leaving the
issue implying the model will grow a team entity.

**R5 open VRP — `extend`, filed as #222.** "No return to the depot" is one bit and the schema
has nowhere to put it: `Route`'s distance and duration both close the tour at `depot_`
(`route.cpp:288`, `route.cpp:249-250`), and there is no field that could say otherwise. PyVRP
solves it with `VehicleType.start_depot` / `end_depot`, which is the more general form and the
one to copy — an open route is a route whose end depot is a zero-cost sink. #222 covers the
whole of what PyVRP's vehicle type has and COSO's lacks, since it is one issue's worth of the
same idea: `start_depot`, `end_depot`, a vehicle shift window (`tw_early` / `tw_late`),
`start_late`, `initial_load`, and `Depot.service_duration`. All additive; owner #178.

**R23 location-routing — `extend`, filed as #223.** LRP is one model with a wider schema, not a
composition: depots become decisions with a fixed cost and a capacity, and the routing is the
same routing. That makes it a scope ruling under principle 2's second sentence, and the scope
answer is yes — it is the routing analogue of the fixed-charge design arc that #184 is adding to
`NetworkModel`, and the facility-location reduction in the network section is the same shape.
Sketch: `DepotParams::fixed_cost`, `DepotParams::capacity`, and a `Result` that says which
depots were opened. Blocked behind [ag]: a model whose every route starts at node 0 cannot
choose a depot. Owner #126, after #196.

**R26 transshipment, R18 split delivery, R21 period VRP, R17 cumulative, R19 time-dependent,
R20 electric, R25 clustered, R16 CARP — `cut`.** Each for its own reason, and none of them for
"no engine has it":

| variant | reason |
|---|---|
| R16 CARP | The demand is on the **arcs**, not the nodes. The standard treatment is a transformation into a node-routing instance before the model is built, so it is a parser and a preprocessing step, not a schema feature — and the transformation is lossy enough (three nodes per required edge) that it belongs to whoever curates the instances, not to `RoutingModel`. Cut as a model feature; a `gdb`/`egl` reader is #119's if it ever wants one |
| R17 cumulative CVRP | The objective is the sum of arrival times, not a route length. Every objective row in this table is a **per-route** cost — distance, duration, fixed, prizes — and a cumulative objective is a per-*client* cost that depends on position in the route. It is a different objective family, `Result` has no arrival times to price it with (§Result), and #173's map has no oracle for it. Cut |
| R18 split delivery | The solution object changes: a client appears in more than one route with a quantity per visit. `Result::routes()` is a list of client ids, `Solution::assigned_` is one bool per client, and `LoadResource` sums a client's whole demand wherever it appears — the representation says a client is served once, everywhere. That is structural, not a missing field. Cut |
| R19 time-dependent VRP | Travel time becomes a function of departure time, so the duration matrix becomes a matrix of functions and every O(1) merge in `DurationResource` stops being O(1). It is a change to the arithmetic of the resource, not to the declaration; the schema would need a piecewise speed profile per arc and `Result` would need departure times. Cut |
| R20 electric VRP | Needs a battery resource, recharge stations as a node class distinct from depot and client, and a nonlinear charging function. Three new entities for one variant, none of which any other variant reuses. Cut |
| R21 period VRP | A horizon plus a visit pattern per client — the solution is one route set *per day*, which `Result` cannot hold, and the choice of pattern is an assignment decision layered on routing. Principle 2's outer loop, over `RoutingModel` per period. Cut |
| R25 clustered VRP | A cluster id per client with the rule that a cluster is served contiguously by one vehicle. It is expressible as a modelling trick — give each cluster's members a mutual `client_type` incompatibility with every other cluster — but not declarable as itself, and the trick needs [n] wired first. No instance set in #177. Cut, and revisit if #196 wires type incompatibility |
| R26 transshipment | Two echelons: freight moves depot → satellite → customer, and the second echelon's supply is the first echelon's delivery. That is two coupled route sets, which is principle 2, and it is also `NetworkModel`'s multi-echelon shape once #184 lands. Cut from this model |

**What PyVRP 0.14.0 has and `RoutingModel` cannot say.** Read off the stubs rather than
inferred, and every row is an `extend` against #178 or a cut with a reason:

| PyVRP | COSO | ruling |
|---|---|---|
| `VehicleType.start_depot` / `end_depot` | every route starts and ends at node 0 | `extend` #222 — this is R4 and R5 in one field pair |
| `VehicleType.tw_early` / `tw_late` / `start_late` | no shift window on a vehicle type | `extend` #222 |
| `VehicleType.initial_load` | a vehicle starts empty | `extend` #222 |
| `Depot.service_duration` | no service time at a depot | `extend` #222 |
| `Shipment` with its own `prize`, `required` and per-step windows | `add_request` is a bare `(pickup, delivery)` pair, and the two clients' `prize` / `required` are independent | `extend`, owner #178 — a request is one optional-or-not unit, and declaring it as two independently-optional clients is not the same problem. Folded into #222 |
| `ClientGroup.required` / `mutually_exclusive` | `add_client_group()` returns an id and stores nothing | already `drops`/dead ([ai]); the PyVRP shape is the target when #196 wires it |
| `Route.schedule()`, `distance_cost()`, `duration_cost()`, `fixed_vehicle_cost()`, `prizes()`, `overtime()`, `num_trips()`, `vehicle_type()`, `start_depot()` | `Result::routes()` is client ids | §Result's contract list; #176 |
| `Solution.is_complete()`, `num_missing_clients()`, `num_missing_groups()` | `feasible()` is load-only | #194 |

And the reverse — what `RoutingModel` says and PyVRP 0.14.0 cannot, which is #178's reject
list: `extra_tw`, `skills` (both), `client_type`, `min_tasks`, `max_tasks`, `speed_factor`,
`set_cost_matrix`, `pin`, and — once #224 lands — sync groups. Every one of those is a `—` in
the PyVRP column above with its reason. `quantity`, `setup_time`, `location` and
`per_task_hour_cost` were on that list too and are deleted instead, which shortens it by four.
**And it is shorter by one more than #200 assumed:** paired pickup-delivery is *not* a reject,
because 0.14.0 has `Shipment`.

**README.** The `RoutingModel` row read
`CVRP, VRPTW, PDPTW, heterogeneous fleet, multi-depot, multi-trip, ...`. Of the six named,
VRPTW, PDPTW, multi-depot and multi-trip are declarable and dropped, heterogeneous fleet cannot
be checked from what is returned, and the `...` claims a rest that this table shows does not
exist. Rewritten in the same commit to the three that have a native `supported` cell —
CVRP and VRPSPD, with multi-dimensional capacity, plus TSP — which has no feature row of its own, its evidence being the variant test — and pointed at this section for the
rest. The engine-status row is corrected in the same commit: "validated against standard CVRP
instances" is true and is also the whole of it, so it now says which declarations the engine
drops rather than leaving "most mature" to imply coverage.

### Defects

| issue | verdict |
|---|---|
| #193 `solve()` ignores `set_initial_routes()` and `pin()` | **dormant** — `src/search/warm_start.h` has `warm_start()`, `PinSet`, `replan()` and `local_search_with_pins()` and no includer. Recorded, not repaired: the disable-and-raise step in that issue is superseded by the current phase's ruling. The principle-4 split above is the modelling half of it, and #176 owns the API change; #178 wires the engine half. Must be zero before the routing milestone closes |
| #194 `Solution::feasible()` checks load only | The single largest defect in this column: it is why `ClientParams::tw`, `DepotParams::tw`, `service`, `max_distance` and `max_duration` are `drops` rather than `supported`, and why R2 and every variant built on it is not expressible. Four `SKIP`-ed tests in `tests/routing/routing_model_test.cpp` name it and hold the assertions that should pass. Recorded, not repaired |
| #196 `solve()` silently drops 21 of 32 fields and four methods | Confirmed by this audit's scripts, with two corrections to its arithmetic. **Five of the 21 are deleted here** rather than kept and rejected — `quantity`, `setup_time`, `location`, `speed_factor`, `per_task_hour_cost` — so 27 slots survive. But the count *rises* rather than falls: this audit reclassifies eight more into `drops` — `ClientParams::tw`, `DepotParams::tw` and `service` under #194, and the five objective slots `cost`, `profile`, `fixed_cost`, `unit_distance_cost`, `unit_duration_cost` under #198 — so the native column is **24 `drops` of 27 slots**, with only `demand`, `pickup` and `capacity` `supported`. Six structural rows are `drops` too, not four: multi-depot, paired pickup-delivery, client groups, the third cost matrix, the reference solution, and explicit node ids. And `prize` is misfiled as dormant-only: it *is* read on the model path (`cost_evaluator.cpp:80`), it simply cannot matter while every client is always served. The disable-and-raise step is superseded by the current phase's ruling — the fields stay declarable and their rows say what happens to them |
| #198 `Result::cost` is total distance, not the declared objective | It is what makes all five objective slots `drops` rather than `supported`, since it removes the only quantity a third party could check them against. Also what makes cross-validation against PyVRP (#178 step 4) compare two different numbers. Recorded, not repaired |
| #220 explicitly-numbered nodes all get the coordinate (0, 0) | **filed by this audit.** `solve()` builds `Coord{d.x, d.y}` regardless of `has_coord` (`routing_model.cpp:133,139`) and `explicit_id` is never read, so a model declared with explicit ids starts from an all-zero distance matrix, every pair the caller did not name stays free, and the granular k-NN lists (`problem_data.cpp:209`) are sorted on that. Not a dropped declaration — a misread one |
| #221 local-search deltas price a different objective than `CostEvaluator` | **filed by this audit.** No move delta prices `unit_duration_cost` (`cost_evaluator.cpp:122-131` skips it by design, justified by the field's default of 0), and `Exchange11` and `Exchange20` build their own deltas with no time-warp term at all, while `Exchange10`, `SwapStar` and `SwapTails` go through `CostEvaluator` and have one. So two of the five neighbourhoods the descent runs are time-window blind, and all five are duration-cost blind. `score_assert` cannot catch it: it recomputes the route's *distance*, not its cost |
| #222 vehicle start/end depot, shift window, initial load, depot service time, `Shipment` | **filed by this audit** as the PyVRP-parity `extend`; it is also R5 (open VRP) and half of R4. Lands with #178 |
| #223 depots as decisions with a fixed cost and a capacity | **filed by this audit** as R23's `extend`; blocked behind #196's multi-depot wiring. Lands with #126 |
| #224 no way to declare synchronised visits | **filed by this audit** as R28's `extend`, with `SyncResource::SyncGroup` already in the tree waiting for it. Lands with #131 |

Two findings with no issue of their own:

- **The existing model-level coverage was not evidence.** Before this audit the only routing
  tests through `RoutingModel::solve()` were `tests/model/model_test.cpp`'s `[routing]` cases,
  which call the setters and assert they return, plus one solve of a single client. That is the
  pattern the evidence rule exists to exclude: every one of them passes whatever the engine does
  with the declaration. "RoutingModel warm start and pin" and "RoutingModel pickup-delivery
  workflow" name features that `solve()` does not implement and never call `solve()`. They are
  kept as API-contract tests — that is what the file is for — and every `supported` cell above
  cites `tests/routing/routing_model_test.cpp` instead.
- **`ProblemData` is a second copy of the schema.** `ProblemData::ClientData` and
  `VehicleTypeData` mirror `ClientParams` and `VehicleTypeParams` field for field, and
  `ProblemData::Builder::add_client` / `add_vehicle_type` copy them across one designated
  initializer at a time. So every schema change is two edits, and a field added to one and not
  the other fails silently at the default. Noted for #176 rather than filed: the plugin protocol
  is where a declaration stops being copied into an engine-specific struct by hand.
