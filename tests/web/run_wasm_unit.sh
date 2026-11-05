#!/usr/bin/env bash
# @spec quality-gates: WebAssembly unit tests
# Runs the complete unit-test suite compiled to WebAssembly in Node.
set -euo pipefail
node "${1:-build/web}/sr_tests.js"
