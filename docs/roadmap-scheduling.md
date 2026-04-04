# Scheduling Roadmap

Content extracted from the [main roadmap](roadmap.md) for the scheduling engine.

---

## API Examples

### Job shop scheduling — same pattern, different domain

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

---

## Attribute Mapping

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

---

## Problem Catalog

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

---

## Instance Formats

| Format | Covers | Parser needed |
|---|---|---|
| Taillard (scheduling) | JSP, PFSP | Yes (Phase 5) |
| OR-Library (scheduling) | JSP | Yes (Phase 5) |
| `.fjs` (FJSPLIB) | FJSP | Yes (Phase 5) |
| `.sm` / `.mm` (PSPLIB) | RCPSP | Yes (Phase 5) |
| Guéret-Prins | OSSP | Yes (Phase 5) |
| MMLIB (multi-mode) | MRCPSP | Yes (Phase 5+) |
| CSPLib prob001 | Car sequencing | Later |

---

## Work Units

### Step 7 — Scheduling engine

```
Deliverable: solve JSP, FJSP, RCPSP from ScheduleModel API.
Touches only src/scheduling/ and src/model/schedule_model.cpp — fully parallel
with step 5 (rich VRP) and step 8 (assignment).
```

| ID | PR title | Deliverable | Files | Depends on | Status |
|----|----------|-------------|-------|------------|--------|
| 7.1 | ScheduleModel implementation | Model → compiled instance for scheduling engine | src/model/schedule_model.cpp | 1.3 | **done** |
| 7.2 | Disjunctive graph data structure | DAG with machine cliques, critical path computation | src/scheduling/disjunctive_graph.{h,cpp} | — | **done** |
| 7.3 | Schedule solution representation | Start times, makespan, Gantt-chart output | src/scheduling/schedule_solution.{h,cpp} | 7.2 | **done** |
| 7.4 | Schedule operators | N5/N7 neighbourhood: swap, insert, block moves | src/scheduling/schedule_operators.{h,cpp} | 7.3 | **done** |
| 7.5 | Construction heuristics (NEH, SGS) | Priority-rule SGS for RCPSP, NEH for flow shop | src/scheduling/construction.{h,cpp} | 7.3 | **done** |
| 7.6 | Mode selection for RCPSP | Multi-mode resource assignment with greedy + local search | src/scheduling/mode_selection.{h,cpp} | 7.3 | **done** |
| 7.7 | Scheduling perturbation | Ruin-and-recreate for scheduling (random block removal, critical path shake, machine reassignment) | src/scheduling/perturbation.{h,cpp} | 7.4 | **done** |
| 7.8 | Scheduling instance parsers | Taillard, PSPLIB, FJSP parsers | src/scheduling/parsers/ | 7.1 | **done** |
| 7.9 | Scheduling benchmarks | Taillard JSP + PSPLIB RCPSP gap tests | tests/scheduling/ | 7.1, 7.4 | **done** |

**7.2, 7.5, 7.6, 7.8 are parallel.** Integration merges at 7.9.

### Step 10 — Advanced features (scheduling)

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 10.8 | Extended scheduling | Setup times, sequence-dependent setups, calendars | src/scheduling/ (extend) | 7.4 | **Done** |
