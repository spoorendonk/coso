# Testing Policy

## Deterministic E2E perf thresholds

The deterministic performance gate compares candidate `work_units` against a
baseline and enforces a median-ratio threshold.

- Tool: `tests/perf/e2e_check_regression.py`
- Threshold policy file: `tests/perf/thresholds.json`
- Default fail condition:
  - fail if median ratio is above threshold
  - fail if candidate/baseline scenario sets differ (unless `--allow-missing`)

Run example (routing pack):

```bash
python3 tests/perf/e2e_check_regression.py \
  --baseline path/to/baseline \
  --candidate path/to/candidate \
  --threshold-policy tests/perf/thresholds.json \
  --pack-key routing
```

The `--pack-key` resolves the threshold from `packs.<key>` and falls back to
`default_max_median_ratio` when the key is not present.

## Quarantine process for unstable scenarios

Quarantine data lives in `tests/e2e/quarantine.csv` with columns:

- `scenario_id`
- `owner`
- `sla_due` (ISO date, `YYYY-MM-DD`)
- `reason`

Behavior:

- pack execution can skip quarantined scenarios when
  `COSO_E2E_APPLY_QUARANTINE=1` is set.
- CI smoke and nightly benchmark lanes enable this flag.
- SLA audit script (`tests/e2e/check_quarantine_sla.py`) fails when any entry
  has a due date earlier than today.

Run audit locally:

```bash
python3 tests/e2e/check_quarantine_sla.py --file tests/e2e/quarantine.csv
```
