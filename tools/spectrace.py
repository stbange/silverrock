#!/usr/bin/env python3
"""Spec-to-test traceability.

Collects every '#### Scenario:' from openspec/specs (implemented behaviour) and from
openspec/changes/*/specs (in flight), and every test tag:
  SR_SPEC("capability", "Scenario name")      in C tests
  @spec capability: Scenario name             in scenario scripts and Playwright tests
plus entries in tests/manual-verification.md ("- capability: Scenario name -- reason").

Main-spec scenarios without a test or a justified manual entry fail; in-flight scenarios are
reported as pending.  Usage: spectrace.py [--root .] [--report out.md]
"""
import argparse
import os
import re
import sys

SCENARIO = re.compile(r"^#### Scenario:\s*(.+?)\s*$")
C_TAG = re.compile(r'SR_SPEC\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\)')
TEXT_TAG = re.compile(r"@spec\s+([a-z0-9-]+):\s*(.+?)\s*$")
MANUAL = re.compile(r"^\s*-\s*([a-z0-9-]+):\s*(.+?)\s+--\s+(.+)$")


def scenarios(root, sub):
    found = {}
    base = os.path.join(root, "openspec", sub)
    for dirpath, _, files in os.walk(base):
        if "archive" in dirpath.split(os.sep):
            continue
        for f in files:
            if f != "spec.md":
                continue
            cap = os.path.basename(dirpath)
            with open(os.path.join(dirpath, f), encoding="utf-8") as fh:
                for line in fh:
                    m = SCENARIO.match(line)
                    if m:
                        found[(cap, m.group(1))] = os.path.join(dirpath, f)
    return found


def tags(root):
    found = set()
    for dirpath, _, files in os.walk(os.path.join(root, "tests")):
        for f in files:
            if not f.endswith((".c", ".h", ".scn", ".js", ".ts", ".mjs", ".py", ".sh")):
                continue
            with open(os.path.join(dirpath, f), encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    for m in (C_TAG.finditer(line) if f.endswith((".c", ".h")) else ()):
                        found.add((m.group(1), m.group(2)))
                    m = TEXT_TAG.search(line)
                    if m:
                        found.add((m.group(1), m.group(2).rstrip("'\";,)")))
    return found


def manual(root):
    found = {}
    path = os.path.join(root, "tests", "manual-verification.md")
    if os.path.exists(path):
        with open(path, encoding="utf-8") as fh:
            for line in fh:
                m = MANUAL.match(line)
                if m:
                    found[(m.group(1), m.group(2))] = m.group(3)
    return found


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--report")
    args = ap.parse_args()
    implemented = scenarios(args.root, "specs")
    pending = scenarios(args.root, "changes")
    tested = tags(args.root)
    manual_ok = manual(args.root)

    def status(key):
        if key in tested:
            return "test"
        if key in manual_ok:
            return "manual"
        return "missing"

    lines = ["# Spec traceability", ""]
    failures = 0
    for title, group, strict in (("Implemented specs", implemented, True), ("Changes in flight", pending, False)):
        total = len(group)
        counts = {"test": 0, "manual": 0, "missing": 0}
        rows = []
        for key in sorted(group):
            s = status(key)
            counts[s] += 1
            if s == "missing":
                rows.append(f"- MISSING {key[0]}: {key[1]}")
                if strict:
                    failures += 1
        lines.append(f"## {title}: {total} scenarios, {counts['test']} tested, "
                     f"{counts['manual']} manual, {counts['missing']} missing")
        lines.extend(rows)
        lines.append("")
    unknown = sorted(t for t in tested if t not in implemented and t not in pending)
    if unknown:
        lines.append("## Tags that match no scenario (typos?)")
        lines.extend(f"- {c}: {s}" for c, s in unknown)
        failures += len(unknown)
    text = "\n".join(lines)
    print(text)
    if args.report:
        with open(args.report, "w", encoding="utf-8") as fh:
            fh.write(text + "\n")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
