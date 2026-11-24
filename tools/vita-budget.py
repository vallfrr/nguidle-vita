#!/usr/bin/env python3
"""Check a console measurement against the versioned budgets (budgets.json) and an optional baseline.

   python tools/vita-budget.py <measurement folder> [--baseline summary.json]

The folder is what tools/vita-cycle.sh writes (work/traces/vita-60/<label>): summary.json and meta.json. Exit code 0 when
every budget holds, 1 when one fails, 2 when the measurement cannot be trusted (no metadata, wrong scenario, no frames).
A figure may be published only with the commit and configuration printed in the last lines of the report.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REQUIRED_META = ("scenario", "vita_commit", "engine_commit", "package", "build_vars")


def check(summary, meta, budgets, baseline=None):
    """Return (code, lines) for one measurement."""
    missing = [key for key in REQUIRED_META if key not in meta]
    if missing:
        return 2, [f"untrusted: meta.json lacks {', '.join(missing)}"]
    budget = budgets.get("scenarios", {}).get(meta["scenario"])
    if budget is None:
        return 2, [f"untrusted: no budget for scenario {meta['scenario']!r} in budgets.json"]
    if summary.get("frames", 0) < 100:
        return 2, [f"untrusted: only {summary.get('frames', 0)} frames in the trace"]
    lines, failed = [], False
    p99 = summary["p99"]
    ok = p99 <= budget["p99_ms"]
    failed |= not ok
    lines.append(f"p99 {p99:.1f} ms (budget {budget['p99_ms']} ms): {'ok' if ok else 'FAIL'}")
    if baseline is not None:
        limit = baseline["p99"] * (1 + budget["max_regression_percent"] / 100)
        ok = p99 <= limit
        failed |= not ok
        lines.append(f"p99 vs baseline {baseline['p99']:.1f} ms (limit {limit:.1f} ms): {'ok' if ok else 'FAIL'}")
    lines.append(
        f"scenario {meta['scenario']}, vita {meta['vita_commit']}, engine {meta['engine_commit']}, package {meta['package']}, "
        f"build {meta['build_vars'] or 'default'}, clocks {meta.get('clocks', 'unknown')}"
    )
    return (1 if failed else 0), lines


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--baseline", type=Path)
    args = parser.parse_args()
    try:
        summary = json.loads((args.folder / "summary.json").read_text(encoding="utf-8"))
        meta = json.loads((args.folder / "meta.json").read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        sys.exit(f"untrusted: {error}")
    budgets = json.loads((ROOT / "budgets.json").read_text(encoding="utf-8"))
    baseline = json.loads(args.baseline.read_text(encoding="utf-8")) if args.baseline else None
    code, lines = check(summary, meta, budgets, baseline)
    print("\n".join(lines))
    sys.exit(code)


if __name__ == "__main__":
    main()
