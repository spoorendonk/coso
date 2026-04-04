# Assignment Roadmap

Content extracted from the [main roadmap](roadmap.md) for the assignment / timetabling engine.

---

## API Examples

### Nurse rostering — assignment with tabu search

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

---

## Attribute Mapping

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

---

## Problem Catalog

| # | Problem | Abbrev | Approach | Phase | Benchmarks | Instances | Source |
|---|---|---|---|---|---|---|---|
| A1 | Nurse Rostering | NRP | Assignment engine (tabu + LA) | 8 | INRC-I, INRC-II, BCV, GPost, SINTEF, ORTEC, Ikegami, Montreal | 100+ | INRC, schedulingbenchmarks.org |
| A2 | Employee Scheduling | ESP | Assignment engine (tabu + LA) | 8 | Curtois shift scheduling (instances 1-24) | 24 | schedulingbenchmarks.org |
| A3 | Multi-Activity Scheduling | MATSP | Assignment engine (tabu + LA) | 8 | Curtois multi-activity multi-day | 225 | schedulingbenchmarks.org |
| A4 | School Timetabling | — | Assignment engine (tabu + LA) | 8+ | ITC-2007, ITC-2019 | varies | ITC |
| A5 | Conference Scheduling | — | Assignment engine (tabu + LA) | 8+ | — | — | — |
| A6 | Bed Allocation | BAS | Assignment engine (tabu + LA) | 8+ | Ceschia-Schaerf | 15+ | Papers |

---

## Instance Formats

| Format | Covers | Parser needed |
|---|---|---|
| INRC-II | NRP | Yes (Phase 6) |
| ITC (timetabling) | School timetabling | Yes (Phase 6) |
| Curtois | Employee scheduling | Yes (Phase 6) |
| BCV/XML (schedulingbenchmarks.org) | NRP | Yes (Phase 8) |
| Curtois shift scheduling | ESP | Yes (Phase 8) |
| Curtois multi-activity | MATSP | Yes (Phase 8) |

---

## Work Units

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
| 8.3 | Basic assignment operators | Swap-shift, move-shift, swap-block between employees | src/assignment/operators/ | 8.2 | **Done** |
| 8.4 | Pillar operators | Multi-employee column moves (VND-style) | src/assignment/operators/pillar_*.{h,cpp} | 8.2 |
| 8.5 | Construction heuristic (FFD) | First-fit-decreasing for initial feasible roster | src/assignment/construction.{h,cpp} | 8.2 |
| 8.6 | Constraint evaluation framework | Incremental soft/hard constraint delta computation | src/assignment/constraints/ | 8.2 |
| 8.7 | Automaton constraint | DFA-based shift pattern rules (e.g., no 3 nights) | src/assignment/constraints/automaton.{h,cpp} | 8.6 |
| 8.8 | CP move filter | Constraint propagation to prune infeasible moves | src/assignment/cp_filter.{h,cpp} | 8.6 |
| 8.9 | Assignment instance parsers | NRP, XML roster format parsers | src/assignment/parsers/ | 8.1 |
| 8.10 | Assignment benchmarks | schedulingbenchmarks.org NRP gap tests | tests/assignment/ | 8.3, 8.9 | **Done** |

**8.3, 8.4, 8.5, 8.6, 8.9 are parallel.** 8.7 and 8.8 depend on 8.6.

### Step 10 — Advanced features (assignment)

| ID | PR title | Deliverable | Files | Depends on |
|----|----------|-------------|-------|------------|
| 10.4 | Assignment replanning | Re-roster with locked shifts and new constraints | src/assignment/ (extend) | Done |
| 10.5 | Overconstrained handling | Soft violations with cost penalties for infeasible instances | src/routing/, src/assignment/ (extend) | 2.4, 8.2 | **Done** |
