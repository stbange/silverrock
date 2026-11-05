#!/usr/bin/env python3
"""C lint gate: clang-format (always) and clang-tidy (with --tidy BUILD_DIR).

Usage: lint_c.py [--clang-format BIN] [--files F ...] [--tidy BUILD_DIR]
Without --files it checks every first-party C file tracked by git (src/, tests/, tools/).
Exit code 1 on any formatting difference or clang-tidy finding."""
import argparse
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def first_party_files():
    out = subprocess.run(["git", "ls-files", "src/*.c", "src/*.h", "tests/*.c", "tests/*.h", "tools/*.c"],
                         cwd=ROOT, capture_output=True, text=True, check=True).stdout.split()
    return [os.path.join(ROOT, f) for f in out if "/fixtures/" not in f]


def check_format(binary, files):
    r = subprocess.run([binary, "--dry-run", "--Werror", "--style=file:" + os.path.join(ROOT, ".clang-format")]
                       + files, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stdout.write(r.stderr[:4000])
        print(f"FAIL: clang-format found misformatted code (run: clang-format -i <files>)")
        return False
    print(f"clang-format: {len(files)} files clean")
    return True


def check_tidy(binary, build):
    with open(os.path.join(build, "compile_commands.json"), encoding="utf-8") as f:
        db = json.load(f)
    files = sorted({e["file"] for e in db
                    if os.path.abspath(e["file"]).startswith(os.path.join(ROOT, "src") + os.sep)})

    def one(path):
        r = subprocess.run([binary, "-p", build, "--quiet", path], capture_output=True, text=True)
        return path, r.returncode, r.stdout

    bad = 0
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 2) as ex:
        for path, rc, out in ex.map(one, files):
            if rc != 0:
                bad += 1
                sys.stdout.write(out)
    if bad:
        print(f"FAIL: clang-tidy reported findings in {bad} file(s)")
        return False
    print(f"clang-tidy: {len(files)} files clean")
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--clang-format", default=os.environ.get("CLANG_FORMAT", "clang-format"))
    ap.add_argument("--clang-tidy", default=os.environ.get("CLANG_TIDY", "clang-tidy"))
    ap.add_argument("--files", nargs="*")
    ap.add_argument("--tidy", metavar="BUILD_DIR")
    args = ap.parse_args()
    ok = check_format(args.clang_format, args.files or first_party_files())
    if args.tidy:
        ok = check_tidy(args.clang_tidy, args.tidy) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
