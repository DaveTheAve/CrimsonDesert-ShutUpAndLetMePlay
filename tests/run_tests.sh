#!/usr/bin/env bash
# Linux x86-64 only. Requires g++ and the exact reference game executable.
# Executes reviewed native routines in isolation; does not launch the game.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ $# -ne 1 || ! -f "$1" ]]; then
  printf 'Usage: bash tests/run_tests.sh "/path/to/reference/CrimsonDesert.exe"\n' >&2
  exit 2
fi
if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
  printf 'Tests require Linux x86-64 (not a Windows or in-game test).\n' >&2
  exit 2
fi
command -v python3 >/dev/null || { printf 'Python 3 is required.\n' >&2; exit 2; }
command -v g++ >/dev/null || { printf 'g++ is required.\n' >&2; exit 2; }
[[ -f "$ROOT/ShutUpAndLetMePlay.asi" ]] || { printf 'Build the ASI first.\n' >&2; exit 2; }
EXPECTED=6d348be9d52f81bd35cf7c55e73a5dbfc96cc8268438387c91f7f62c82381fa7
ACTUAL="$(sha256sum -- "$1")"
if [[ "${ACTUAL%% *}" != "$EXPECTED" ]]; then
  printf 'The isolation tests require the exact reference executable listed in tests/reference.json.\n' >&2
  exit 2
fi
BUILD="$(mktemp -d)"
trap 'rm -rf -- "$BUILD"' EXIT
CXXFLAGS=(-std=c++17 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation)
g++ "${CXXFLAGS[@]}" "$ROOT/tests/native_harness.cpp" -o "$BUILD/native_harness"
g++ "${CXXFLAGS[@]}" "$ROOT/tests/compiled_asi_harness.cpp" -o "$BUILD/compiled_asi_harness"
"$BUILD/native_harness" "$1"
for scenario in success busy allocation unwind protect suspend context flush helper duplicate duplicate_after case mutex pin image shape short write zero rename readonly; do
  "$BUILD/compiled_asi_harness" "$1" "$ROOT/ShutUpAndLetMePlay.asi" "$scenario" "$BUILD/reports"
done
python3 "$ROOT/tests/validate_reports.py" "$BUILD/reports"
