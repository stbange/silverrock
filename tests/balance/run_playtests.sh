#!/usr/bin/env bash
# Nightly playtests: 16 characters x 26 weeks, gated by tests/balance/playtest_bands.txt.
# Usage: run_playtests.sh <sr_headless> <out-dir>
# @spec character-progression: Paced rise
# @spec job-system: Money keeps mattering
# @spec job-system: Approaches stay balanced
# @spec story-campaign: Paced campaign
set -euo pipefail
headless="$1"
out="$2"
mkdir -p "$out"
"$headless" --playtest 16 "$out" --bands "$(dirname "$0")/playtest_bands.txt" > "$out/summary.csv"
