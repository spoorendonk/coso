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
