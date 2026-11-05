#!/usr/bin/env python3
"""Self-tests for the quality gates: every gate must fail on a planted problem.
Usage: python3 tests/tools/test_tools.py [build-dir]   (build-dir has sr_headless)"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TOOLS = os.path.join(ROOT, "tools")
BUILD = os.environ.get("SR_BUILD", os.path.join(ROOT, "build", "dev"))


def run(args, cwd=None):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True)


def gcovr_json(files):
    return {"files": [{"file": f, "lines": [{"count": c} for c in counts]} for f, counts in files.items()]}


class CoverageGate(unittest.TestCase):
    def gate(self, report, src=None):
        d = tempfile.mkdtemp()
        os.makedirs(os.path.join(d, "src", "core"))
        with open(os.path.join(d, "src", "core", "a.c"), "w") as f:
            f.write(src or "int a(void) { return 1; }\n" * 200)
        rp = os.path.join(d, "r.json")
        with open(rp, "w") as f:
            json.dump(report, f)
        r = run([sys.executable, os.path.join(TOOLS, "coverage_gate.py"), "x", "--root", d, "--json", rp])
        shutil.rmtree(d)
        return r

    def test_below_threshold(self):
        # @spec quality-gates: Coverage below threshold
        r = self.gate(gcovr_json({"src/core/a.c": [1] * 949 + [0] * 51}))
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("below", r.stdout)

    def test_directory_minimum(self):
        # @spec quality-gates: Directory below minimum
        r = self.gate(gcovr_json({"src/core/a.c": [1] * 1000, "src/sim/b.c": [1] * 89 + [0] * 11}))
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("src/sim", r.stdout)

    def test_third_party_not_counted(self):
        # @spec quality-gates: Third-party code is not counted
        r = self.gate(gcovr_json({"src/core/a.c": [1] * 100, "third_party/SDL/x.c": [0] * 10000}))
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn("100.00%", r.stdout)

    def test_exclusion_without_reason(self):
        # @spec quality-gates: Exclusion without a reason
        src = "int a(void) { return 1; }\n" * 200 + "int b(void) { return 2; } /* GCOVR_EXCL_LINE */\n"
        r = self.gate(gcovr_json({"src/core/a.c": [1] * 100}), src)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("without a reason", r.stdout)

    def test_exclusion_budget(self):
        # @spec quality-gates: Exclusion budget exceeded
        src = "int a(void) { return 1; }\n" * 50 + "/* GCOVR_EXCL_START reason: test */\n" + "x;\n" * 10 + "/* GCOVR_EXCL_STOP */\n"
        r = self.gate(gcovr_json({"src/core/a.c": [1] * 100}), src)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("exceed", r.stdout)


class Traceability(unittest.TestCase):
    def tree(self, implemented, pending, tests):
        d = tempfile.mkdtemp()
        for base, scen in (("specs/cap", implemented), ("changes/c1/specs/cap2", pending)):
            os.makedirs(os.path.join(d, "openspec", base))
            with open(os.path.join(d, "openspec", base, "spec.md"), "w") as f:
                f.write("".join(f"#### Scenario: {s}\n" for s in scen))
        os.makedirs(os.path.join(d, "tests"))
        with open(os.path.join(d, "tests", "t.c"), "w") as f:
            f.write(tests)
        r = run([sys.executable, os.path.join(TOOLS, "spectrace.py"), "--root", d])
        shutil.rmtree(d)
        return r

    def test_untested_scenario_fails(self):
        # @spec quality-gates: Untested scenario
        r = self.tree(["Covered", "Forgotten"], [], 'SR_SPEC("cap", "Covered");')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("MISSING cap: Forgotten", r.stdout)

    def test_change_in_progress_is_pending(self):
        # @spec quality-gates: Change in progress
        r = self.tree(["Covered"], ["Not yet"], 'SR_SPEC("cap", "Covered");')
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn("MISSING cap2: Not yet", r.stdout)


class CLint(unittest.TestCase):
    def test_misformatted_fixture_fails(self):
        fixture = os.path.join(ROOT, "tests", "tools", "fixtures", "misformatted.c")
        r = run([sys.executable, os.path.join(TOOLS, "lint_c.py"), "--files", fixture])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("misformatted", r.stdout)

    def test_repository_is_formatted(self):
        r = run([sys.executable, os.path.join(TOOLS, "lint_c.py")])
        self.assertEqual(r.returncode, 0, r.stdout)


class LayerCheck(unittest.TestCase):
    def test_forbidden_include(self):
        # @spec quality-gates: Forbidden include
        d = tempfile.mkdtemp()
        os.makedirs(os.path.join(d, "src", "sim"))
        with open(os.path.join(d, "src", "sim", "x.c"), "w") as f:
            f.write("#include <SDL3/SDL.h>\n#include \"platform/sr_platform.h\"\n")
        r = run([sys.executable, os.path.join(TOOLS, "layercheck.py"), d])
        shutil.rmtree(d)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("SDL3/SDL.h", r.stdout)
        self.assertIn("higher layer", r.stdout)


class Builds(unittest.TestCase):
    def test_failing_game_test_fails(self):
        # @spec quality-gates: Failing game test blocks the merge
        exe = os.path.join(BUILD, "sr_headless")
        if not os.path.exists(exe):
            self.skipTest("no headless build")
        r = run([exe, "--script", os.path.join(ROOT, "tests", "game", "headless_fail.scn.txt")])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("headless_fail.scn.txt:4", r.stderr)

    def test_balance_bands_gate(self):
        exe = os.path.join(BUILD, "sr_headless")
        if not os.path.exists(exe):
            self.skipTest("no headless build")
        d = tempfile.mkdtemp()
        tight = os.path.join(d, "bands.txt")
        with open(tight, "w") as f:
            f.write("# impossible band\ngig earned 999999 9999999\n* dominance 0 100\n")
        r = run([exe, "--balance", "1", "--bands", tight])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("BAND FAIL gig earned", r.stdout)
        with open(tight, "w") as f:
            f.write("gig earned 0 9999999\n")
        r = run([exe, "--balance", "1", "--bands", tight])
        self.assertEqual(r.returncode, 0, r.stdout)
        r = run([exe, "--balance", "1", "--bands", os.path.join(d, "missing.txt")])
        self.assertNotEqual(r.returncode, 0)
        shutil.rmtree(d)

    def test_new_warning_fails(self):
        # @spec quality-gates: New compiler warning
        d = tempfile.mkdtemp()
        p = os.path.join(d, "w.c")
        with open(p, "w") as f:
            f.write("int f(void) { int unused; return 0; }\n")
        r = run(["cc", "-std=c17", "-Wall", "-Wextra", "-Werror", "-c", p, "-o", os.path.join(d, "w.o")])
        shutil.rmtree(d)
        self.assertNotEqual(r.returncode, 0)

    def test_sanitizer_catches_memory_error(self):
        # @spec quality-gates: Memory error detected
        d = tempfile.mkdtemp()
        p = os.path.join(d, "m.c")
        with open(p, "w") as f:
            f.write("#include <stdlib.h>\nint main(void){char*p=malloc(4);p[4]=1;free(p);return 0;}\n")
        exe = os.path.join(d, "m")
        if run(["cc", "-g", "-fsanitize=address,undefined", p, "-o", exe]).returncode != 0:
            shutil.rmtree(d)
            self.skipTest("no sanitizer runtime")
        r = run([exe])
        shutil.rmtree(d)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("AddressSanitizer", r.stderr)

    def test_fuzzer_finds_planted_crash(self):
        # @spec quality-gates: Crash found by the fuzzer
        d = tempfile.mkdtemp()
        exe = os.path.join(d, "fz")
        src = [os.path.join(ROOT, "tests", "fuzz", "crash_fixture.c"), os.path.join(ROOT, "tests", "fuzz", "fuzz_driver.c")]
        if run(["cc", "-g", "-fsanitize=address"] + src + ["-o", exe]).returncode != 0:
            shutil.rmtree(d)
            self.skipTest("no sanitizer runtime")
        r = run([exe, "200000", "7"], cwd=d)
        reproducer = os.path.exists(os.path.join(d, "crash-input.bin"))
        shutil.rmtree(d)
        self.assertNotEqual(r.returncode, 0)
        self.assertTrue(reproducer, "reproducing input attached")

    def test_release_symbol_check(self):
        # @spec quality-gates: Release symbol check
        d = tempfile.mkdtemp()
        good, bad = os.path.join(d, "good.bin"), os.path.join(d, "bad.bin")
        open(good, "wb").write(b"\0main\0sr_game_new\0")
        open(bad, "wb").write(b"\0main\0sr_app_test_exec\0")
        ok = run([sys.executable, os.path.join(TOOLS, "symbol_check.py"), good])
        fail = run([sys.executable, os.path.join(TOOLS, "symbol_check.py"), bad])
        shutil.rmtree(d)
        self.assertEqual(ok.returncode, 0)
        self.assertNotEqual(fail.returncode, 0)

    def test_web_size_check(self):
        # @spec platform-runtime: Release size check
        d = tempfile.mkdtemp()
        open(os.path.join(d, "silverrock.wasm"), "wb").write(os.urandom(300 * 1024))
        small = run([sys.executable, os.path.join(TOOLS, "web_size_check.py"), d, "--budget-mb", "1"])
        big = run([sys.executable, os.path.join(TOOLS, "web_size_check.py"), d, "--budget-mb", "0.1"])
        shutil.rmtree(d)
        self.assertEqual(small.returncode, 0)
        self.assertNotEqual(big.returncode, 0)

    def test_openspec_rejects_malformed(self):
        # @spec quality-gates: Malformed scenario
        cli = shutil.which("openspec")
        if not cli:
            self.skipTest("openspec CLI not installed")
        d = tempfile.mkdtemp()
        os.makedirs(os.path.join(d, "openspec", "specs", "bad"))
        os.makedirs(os.path.join(d, "openspec", "changes"))
        with open(os.path.join(d, "openspec", "config.yaml"), "w") as f:
            f.write("schema: spec-driven\n")
        with open(os.path.join(d, "openspec", "specs", "bad", "spec.md"), "w") as f:
            f.write("# bad Specification\n\n## Purpose\nA capability used to check that validation fails.\n\n"
                    "## Requirements\n\n### Requirement: No scenario\nThe system SHALL do something.\n")
        r = run([cli, "validate", "--all", "--strict", "--no-interactive"], cwd=d)
        shutil.rmtree(d)
        self.assertNotEqual(r.returncode, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
