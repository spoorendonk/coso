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

**Deletion rule.** A `declarable` feature is deleted from the schema when no engine column is
`supported` or `documented`, unless the native column's dormant code has an owner committed to
wiring it in a named milestone. An idle header is not an owner. Everything kept stays
declarable, and the native engine `rejects` it (the disable-and-raise fixes in #193, #195,
#196) until wired.

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
engine is integrated; nothing else is a dependency of this repo, so the other three columns can
hold no value but `documented`, pinned to a public API reference:

- OR-Tools v9.11, `ortools/graph/min_cost_flow.h` (`SimpleMinCostFlow`).
- mcfcg (<https://github.com/spoorendonk/mcfcg>, arXiv:2509.24656) — column generation for
  min-cost multicommodity flow, path- and tree-based Dantzig-Wolfe; README formulations.
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
| `feasible()` | conservation at every node and `lower_cap <= flow <= upper_cap` on every arc |
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
  `PackingSolution::item_fits_capacity` and by the same loop in the move/swap/merge/split
  feasibility checks — **not** by `src/packing/bin_capacity.h`, the file #168 named: nothing on
  the model path constructs a `BinCapacity`, only `tests/packing/bin_capacity_test.cpp` does.
- [b] `tests/packing/packing_model_test.cpp` "PackingModel: variable-sized bin packing costs the
  mix": a small type (capacity 5, cost 1) and a large one (capacity 10, cost 5) with items 8, 5,
  5 return three bins costing 7, where the control section — large bins only — returns the two
  bins costing 10 that a bin-*count* objective would prefer.
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
  `PackingSolution::has_conflict_in_bin` through `item_fits`, and by the conflict checks in
  every operator's `is_feasible`.
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
2. **`Result::unassigned_` is documented as assignment-only.** In `src/model/types.h` the field
   sits in the "Assignment (nurse rostering)" block, but `PackingModel::solve()` populates it
   and packing has no other channel for unpacked items — [c]'s two unassigned items arrive
   there. The field is shared; its comment is not. Noted on #176.

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
  against Falkenauer" (#167, #172, README) overstates what runs.
- **#170 (BPPLIB parser) and #171 (Python bindings for `PackingModel`) are closed as completed,
  and neither exists in the tree.** There is no packing parser anywhere in `src/`, and
  `python/bindings.cpp` binds `RoutingModel`, `NetworkModel` and `LotSizingModel` only. Nothing
  in this audit depends on either, and no Python surface is affected by the cut above — but
  their closed state is not a record of work done, and #182 should not plan around them.
