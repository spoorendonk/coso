#!/usr/bin/env python3
"""Audit quarantine SLA deadlines for E2E scenarios."""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Fail when quarantined scenarios have expired SLA dates."
    )
    parser.add_argument(
        "--file",
        type=Path,
        default=Path("tests/e2e/quarantine.csv"),
    )
    args = parser.parse_args()

    if not args.file.exists():
        print(f"error: quarantine file not found: {args.file}", file=sys.stderr)
        return 1

    today = dt.date.today()
    overdue: list[tuple[str, str, dt.date, str]] = []

    with args.file.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        expected = {"scenario_id", "owner", "sla_due", "reason"}
        if set(reader.fieldnames or []) != expected:
            print(
                "error: quarantine file must have header: "
                "scenario_id,owner,sla_due,reason",
                file=sys.stderr,
            )
            return 1

        for row in reader:
            scenario_id = (row.get("scenario_id") or "").strip()
            owner = (row.get("owner") or "").strip()
            due_raw = (row.get("sla_due") or "").strip()
            reason = (row.get("reason") or "").strip()

            if not scenario_id:
                continue
            if scenario_id.startswith("#"):
                continue
            if not owner:
                print(f"error: missing owner for quarantined scenario {scenario_id}")
                return 1
            if not due_raw:
                print(f"error: missing sla_due for quarantined scenario {scenario_id}")
                return 1
            try:
                due = dt.date.fromisoformat(due_raw)
            except ValueError:
                print(
                    f"error: invalid sla_due '{due_raw}' for scenario {scenario_id} "
                    "(expected YYYY-MM-DD)",
                    file=sys.stderr,
                )
                return 1

            if due < today:
                overdue.append((scenario_id, owner, due, reason))

    if overdue:
        print("FAIL: quarantined scenarios with expired SLA:")
        for scenario_id, owner, due, reason in overdue:
            print(f"  - {scenario_id} owner={owner} due={due} reason={reason}")
        return 2

    print("PASS: quarantine SLA audit")
    return 0


if __name__ == "__main__":
    sys.exit(main())
