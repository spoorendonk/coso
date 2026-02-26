# E2E Contribution Guide

This guide defines the minimum bar for adding or updating E2E scenarios.

## 1. Pick and claim a work unit

- Choose the roadmap unit (`14.x`, `15.x`, or related support lane).
- Update `docs/e2e-matrix.md` for affected rows:
  - set `Owner`
  - set `Status=in_progress`

## 2. Add scenario files

- Place scenarios under `examples/e2e/scenarios/<pack>/{smoke,benchmark}/`.
- Follow naming:
  - smoke: `smoke_<variant>_<name>_01.json`
  - benchmark: `bench_<variant>_<name>_01.json`
- Required fields:
  - `id`
  - `model`
  - `time_limit.seconds`
- Recommended fields:
  - `variant`
  - `profile`
  - `time_limit.work_units`
  - `checks`

## 3. Register execution

- Register pack tests in `tests/CMakeLists.txt` (or extend existing pack lane).
- Required labels:
  - smoke jobs must carry `e2e-smoke`
  - benchmark jobs must carry `e2e-benchmark` and `benchmark`

## 4. Deterministic checks and perf policy

- Include `deterministic_work` in scenario checks unless explicitly exempted.
- For perf gates, use `tests/perf/e2e_check_regression.py`.
- Threshold policy source of truth: `tests/perf/thresholds.json`.

## 5. Quarantine policy

- Unstable scenarios go into `tests/e2e/quarantine.csv` with:
  - `scenario_id`
  - `owner`
  - `sla_due`
  - `reason`
- Keep quarantine temporary and audit with:
  - `python3 tests/e2e/check_quarantine_sla.py --file tests/e2e/quarantine.csv`

## 6. Validation commands

```bash
cmake --build build -j$(nproc)
ctest --test-dir build -L e2e-smoke --output-on-failure
ctest --test-dir build -L e2e-benchmark --output-on-failure
ctest --test-dir build -R e2e_perf_gate --output-on-failure
```

## 7. Completion criteria

A row in `docs/e2e-matrix.md` can move to `done` only when:

- scenario files exist,
- required checks are present,
- scenario is in a smoke or benchmark CTest lane,
- CI evidence is attached in the PR.
