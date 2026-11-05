#!/usr/bin/env python3
"""Coverage gate: >= 95 % line coverage overall, >= 90 % per source directory, and coverage
exclusions capped at 1.5 % of first-party lines, each with a written reason.

Usage: coverage_gate.py <build-dir> [--root .] [--min 95] [--min-dir 90] [--max-excluded 1.5]
Requires gcovr. Third-party code (third_party/) is never counted.
"""
import argparse
import json
import os
import re
import subprocess
import sys
from collections import defaultdict

EXCL_LINE = re.compile(r"GCOVR_EXCL_LINE(.*)")
EXCL_START = re.compile(r"GCOVR_EXCL_START(.*)")
EXCL_STOP = re.compile(r"GCOVR_EXCL_STOP")


def exclusions(root):
    """Returns (excluded_lines, total_lines, problems) over first-party sources."""
    excluded, total, problems = 0, 0, []
    for base, _, files in os.walk(os.path.join(root, "src")):
        for name in files:
            if not name.endswith((".c", ".h")):
                continue
            path = os.path.join(base, name)
            with open(path, encoding="utf-8") as f:
                lines = f.readlines()
            total += len(lines)
            inside = False
            for no, line in enumerate(lines, 1):
                m = EXCL_START.search(line)
                if m:
                    inside = True
                    if "reason:" not in m.group(1):
                        problems.append(f"{path}:{no}: exclusion without a reason")
                    excluded += 1
                    continue
                if EXCL_STOP.search(line):
                    inside = False
                    excluded += 1
                    continue
                if inside:
                    excluded += 1
                    continue
                m = EXCL_LINE.search(line)
                if m:
                    excluded += 1
                    reason = m.group(1)
                    # a continuation marker refers to the reason on the line above
                    if "reason:" not in reason and reason.strip() not in ("*/", ""):
                        problems.append(f"{path}:{no}: exclusion without a reason")
                    elif "reason:" not in reason:
                        prev = lines[no - 2] if no >= 2 else ""
                        if "reason:" not in prev:
                            problems.append(f"{path}:{no}: exclusion without a reason")
    return excluded, total, problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("--root", default=".")
    ap.add_argument("--min", type=float, default=95.0)
    ap.add_argument("--min-dir", type=float, default=90.0)
    ap.add_argument("--max-excluded", type=float, default=1.5)
    ap.add_argument("--json", help="use an existing gcovr JSON report instead of running gcovr")
    args = ap.parse_args()

    if args.json:
        with open(args.json, encoding="utf-8") as f:
            report = json.load(f)
    else:
        out = subprocess.run(
            ["gcovr", "-r", args.root, "--filter", "src/",
             "--gcov-ignore-parse-errors=suspicious_hits.warn", "--json", "-", args.build],
            check=True, capture_output=True, text=True).stdout
        report = json.loads(out)

    per_dir = defaultdict(lambda: [0, 0])
    total_lines = covered_lines = 0
    for fentry in report["files"]:
        name = fentry["file"]
        if "third_party" in name:
            continue
        lines = [l for l in fentry["lines"]
                 if not l.get("gcovr/noncode") and not l.get("gcovr/excluded")]
        n = len(lines)
        if n == 0:
            continue  # fully excluded file (e.g. the entry point)
        c = sum(1 for l in lines if l["count"] > 0)
        d = os.path.dirname(name)
        per_dir[d][0] += c
        per_dir[d][1] += n
        total_lines += n
        covered_lines += c

    ok = True
    overall = 100.0 * covered_lines / max(1, total_lines)
    print(f"overall line coverage: {overall:.2f}% ({covered_lines}/{total_lines})")
    if overall < args.min:
        print(f"FAIL: overall coverage below {args.min}%")
        ok = False
    for d in sorted(per_dir):
        c, n = per_dir[d]
        pct = 100.0 * c / max(1, n)
        flag = "" if pct >= args.min_dir else "  <-- below minimum"
        print(f"  {d:<20} {pct:6.2f}% ({c}/{n}){flag}")
        if pct < args.min_dir:
            ok = False

    excluded, src_lines, problems = exclusions(args.root)
    share = 100.0 * excluded / max(1, src_lines)
    print(f"excluded lines: {excluded} of {src_lines} source lines ({share:.2f}%)")
    for p in problems:
        print("FAIL:", p)
        ok = False
    if share > args.max_excluded:
        print(f"FAIL: exclusions exceed {args.max_excluded}%")
        ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
