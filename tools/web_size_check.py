#!/usr/bin/env python3
"""Fails when the compressed size of the files needed to reach the main menu exceeds a budget.
Usage: web_size_check.py <web-build-dir> [--budget-mb 20]"""
import argparse, gzip, os, sys

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--budget-mb", type=float, default=20.0)
    a = ap.parse_args()
    total = 0
    for name in sorted(os.listdir(a.dir)):
        if not name.endswith((".html", ".js", ".wasm", ".data", ".webmanifest")):
            continue
        with open(os.path.join(a.dir, name), "rb") as f:
            size = len(gzip.compress(f.read(), 9))
        total += size
        print(f"  {name:<28} {size / 1024:9.1f} KiB compressed")
    mb = total / (1024 * 1024)
    print(f"total {mb:.2f} MiB compressed (budget {a.budget_mb} MiB)")
    return 0 if mb <= a.budget_mb else 1

if __name__ == "__main__":
    sys.exit(main())
