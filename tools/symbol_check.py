#!/usr/bin/env python3
"""Release symbol check: fails if a binary or Wasm/JS output contains the test interface.
Usage: symbol_check.py <file>..."""
import sys

MARKERS = [b"sr_app_test_exec", b"sr_web_test_exec", b"sr_game_command"]

def main(paths):
    bad = 0
    for p in paths:
        data = open(p, "rb").read()
        found = [m.decode() for m in MARKERS if m in data]
        if found:
            print(f"FAIL {p}: contains test interface {found}")
            bad += 1
        else:
            print(f"ok   {p}")
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
