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
