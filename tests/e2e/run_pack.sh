#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: run_pack.sh <e2e_runner> <scenario_dir>" >&2
  exit 1
fi

runner="$1"
scenario_dir="$2"

if [[ ! -x "$runner" ]]; then
  echo "Runner not executable: $runner" >&2
  exit 1
fi

if [[ ! -d "$scenario_dir" ]]; then
  echo "Scenario directory missing: $scenario_dir" >&2
  exit 1
fi

status=0
found=0
for scenario in "$scenario_dir"/*.json; do
  if [[ ! -f "$scenario" ]]; then
    continue
  fi
  found=1
  echo "[e2e] running: ${scenario}"
  if ! "$runner" "$scenario"; then
    status=1
  fi
done

if [[ $found -eq 0 ]]; then
  echo "No scenario JSON files found in: $scenario_dir" >&2
  exit 1
fi

exit $status
