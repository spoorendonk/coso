#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: run_pack.sh <e2e_runner> <scenario_dir>" >&2
  exit 1
fi

runner="$1"
scenario_dir="$2"
apply_quarantine="${COSO_E2E_APPLY_QUARANTINE:-0}"
quarantine_file="${COSO_E2E_QUARANTINE_FILE:-$(dirname "$0")/quarantine.csv}"

if [[ ! -x "$runner" ]]; then
  echo "Runner not executable: $runner" >&2
  exit 1
fi

if [[ ! -d "$scenario_dir" ]]; then
  echo "Scenario directory missing: $scenario_dir" >&2
  exit 1
fi

declare -A quarantine_owner
declare -A quarantine_due
declare -A quarantine_reason
if [[ "$apply_quarantine" == "1" ]]; then
  if [[ -f "$quarantine_file" ]]; then
    while IFS=, read -r scenario_id owner sla_due reason; do
      [[ -z "$scenario_id" ]] && continue
      [[ "$scenario_id" == "scenario_id" ]] && continue
      [[ "$scenario_id" =~ ^# ]] && continue
      quarantine_owner["$scenario_id"]="$owner"
      quarantine_due["$scenario_id"]="$sla_due"
      quarantine_reason["$scenario_id"]="$reason"
    done < "$quarantine_file"
  else
    echo "[e2e] quarantine file not found, continuing without quarantine: $quarantine_file" >&2
  fi
fi

extract_id() {
  local file="$1"
  sed -n 's/^[[:space:]]*"id"[[:space:]]*:[[:space:]]*"\([^"]\+\)".*/\1/p' "$file" | head -n 1
}

status=0
found=0
quarantined=0
for scenario in "$scenario_dir"/*.json; do
  if [[ ! -f "$scenario" ]]; then
    continue
  fi
  found=1
  scenario_id="$(extract_id "$scenario")"
  if [[ "$apply_quarantine" == "1" ]] && [[ -n "${scenario_id}" ]] && [[ -n "${quarantine_owner[$scenario_id]+x}" ]]; then
    echo "[e2e] quarantined: ${scenario} (id=${scenario_id}, owner=${quarantine_owner[$scenario_id]}, due=${quarantine_due[$scenario_id]}, reason=${quarantine_reason[$scenario_id]})"
    quarantined=$((quarantined + 1))
    continue
  fi
  echo "[e2e] running: ${scenario}"
  if ! "$runner" "$scenario"; then
    status=1
  fi
done

if [[ $found -eq 0 ]]; then
  echo "No scenario JSON files found in: $scenario_dir" >&2
  exit 1
fi

if [[ "$apply_quarantine" == "1" ]] && [[ $quarantined -gt 0 ]]; then
  echo "[e2e] skipped quarantined scenarios: $quarantined"
fi

exit $status
