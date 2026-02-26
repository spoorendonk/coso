#!/usr/bin/env python3
"""Deterministic E2E performance gate on work_units median ratio.

Compares candidate results against baseline results and fails when the median
work_units ratio exceeds the configured threshold.
"""

from __future__ import annotations

import argparse
import json
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ScenarioSample:
    scenario_id: str
    work_units: float


def parse_payloads(path: Path) -> list[dict]:
    text = path.read_text(encoding="utf-8").strip()
    if not text:
        return []

    try:
        payload = json.loads(text)
        if isinstance(payload, list):
            return [item for item in payload if isinstance(item, dict)]
        if isinstance(payload, dict):
            return [payload]
    except json.JSONDecodeError:
        pass

    payloads: list[dict] = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        value = json.loads(line)
        if isinstance(value, dict):
            payloads.append(value)
    return payloads


def extract_sample(payload: dict, source: Path) -> ScenarioSample:
    scenario_id = payload.get("scenario_id")
    result = payload.get("result", {})
    work_units = result.get("work_units")

    if not isinstance(scenario_id, str) or not scenario_id:
        raise ValueError(f"{source}: missing string scenario_id")
    if not isinstance(work_units, (int, float)):
        raise ValueError(f"{source}: missing numeric result.work_units")

    return ScenarioSample(scenario_id=scenario_id, work_units=float(work_units))


def collect_samples(root: Path) -> dict[str, float]:
    files: list[Path]
    if root.is_dir():
        files = sorted(p for p in root.rglob("*.json") if p.is_file())
    elif root.is_file():
        files = [root]
    else:
        raise ValueError(f"path not found: {root}")

    samples: dict[str, float] = {}
    for file in files:
        payloads = parse_payloads(file)
        if not payloads:
            continue
        for payload in payloads:
            sample = extract_sample(payload, file)
            if sample.scenario_id in samples:
                raise ValueError(
                    f"duplicate scenario_id '{sample.scenario_id}' in {file}"
                )
            samples[sample.scenario_id] = sample.work_units
    if not samples:
        raise ValueError(f"no scenario samples found under: {root}")
    return samples


def format_ratio(value: float) -> str:
    return f"{value:.4f}x"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check candidate E2E work_units regression vs baseline."
    )
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--candidate", required=True, type=Path)
    parser.add_argument("--max-median-ratio", type=float, default=1.10)
    parser.add_argument("--allow-missing", action="store_true")
    args = parser.parse_args()

    if args.max_median_ratio <= 0.0:
        print("error: --max-median-ratio must be positive", file=sys.stderr)
        return 1

    try:
        baseline = collect_samples(args.baseline)
        candidate = collect_samples(args.candidate)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    baseline_ids = set(baseline)
    candidate_ids = set(candidate)
    missing = sorted(baseline_ids - candidate_ids)
    added = sorted(candidate_ids - baseline_ids)

    if (missing or added) and not args.allow_missing:
        if missing:
            print("error: missing scenarios in candidate:", file=sys.stderr)
            for scenario_id in missing:
                print(f"  - {scenario_id}", file=sys.stderr)
        if added:
            print("error: unexpected scenarios in candidate:", file=sys.stderr)
            for scenario_id in added:
                print(f"  - {scenario_id}", file=sys.stderr)
        return 2

    shared = sorted(baseline_ids & candidate_ids)
    ratios: list[tuple[str, float]] = []
    skipped_zero_baseline: list[str] = []
    for scenario_id in shared:
        base = baseline[scenario_id]
        cand = candidate[scenario_id]
        if base <= 0.0:
            skipped_zero_baseline.append(scenario_id)
            continue
        ratios.append((scenario_id, cand / base))

    if not ratios:
        print("error: no comparable scenarios with positive baseline work_units")
        return 1

    median_ratio = statistics.median(ratio for _, ratio in ratios)
    max_ratio = max(ratio for _, ratio in ratios)

    print("E2E deterministic work_units regression")
    print(f"baseline:  {args.baseline}")
    print(f"candidate: {args.candidate}")
    print(f"scenarios compared: {len(ratios)}")
    print(f"median ratio: {format_ratio(median_ratio)}")
    print(f"max ratio:    {format_ratio(max_ratio)}")
    if skipped_zero_baseline:
        print("skipped (baseline<=0): " + ", ".join(skipped_zero_baseline))

    status = 0
    if median_ratio > args.max_median_ratio:
        print(
            f"FAIL: median ratio {format_ratio(median_ratio)} > threshold "
            f"{format_ratio(args.max_median_ratio)}"
        )
        status = 2
    else:
        print(
            f"PASS: median ratio {format_ratio(median_ratio)} <= threshold "
            f"{format_ratio(args.max_median_ratio)}"
        )

    print("\nPer-scenario ratios:")
    for scenario_id, ratio in ratios:
        print(f"  {scenario_id}: {format_ratio(ratio)}")

    return status


if __name__ == "__main__":
    sys.exit(main())
