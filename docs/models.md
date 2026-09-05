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
