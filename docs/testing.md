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
