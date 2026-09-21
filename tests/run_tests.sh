#!/usr/bin/env bash
# Linux x86-64 only. Requires g++ and a listed exact reference game executable.
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
python3 - "$ROOT/tests/reference.json" "$1" <<'PYREF'
import hashlib, json, pathlib, sys
reference = json.loads(pathlib.Path(sys.argv[1]).read_text())
known = [reference] + reference.get('additional_references', [])
digest = hashlib.sha256()
with open(sys.argv[2], 'rb') as executable:
    for chunk in iter(lambda: executable.read(1024 * 1024), b''):
        digest.update(chunk)
matches = [r for r in known if r['executable_sha256'] == digest.hexdigest()]
if len(matches) != 1:
    raise SystemExit('Isolation tests require an exact executable listed in tests/reference.json.')
print('Verified isolation-test reference:', matches[0]['file_version'], digest.hexdigest())
PYREF
BUILD="$(mktemp -d)"
trap 'rm -rf -- "$BUILD"' EXIT
CXXFLAGS=(-std=c++17 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-unused-function)
g++ "${CXXFLAGS[@]}" "$ROOT/tests/native_harness.cpp" -o "$BUILD/native_harness"
g++ "${CXXFLAGS[@]}" "$ROOT/tests/compiled_asi_harness.cpp" -o "$BUILD/compiled_asi_harness"
g++ "${CXXFLAGS[@]}" "$ROOT/tests/dialogue_harness.cpp" -o "$BUILD/dialogue_harness"
g++ "${CXXFLAGS[@]}" "$ROOT/tests/dialogue_compiled_harness.cpp" -o "$BUILD/dialogue_compiled_harness"
g++ "${CXXFLAGS[@]}" "$ROOT/tests/sequencer_compiled_harness.cpp" -o "$BUILD/sequencer_compiled_harness"
"$BUILD/native_harness" "$1"
"$BUILD/dialogue_harness" "$1"
for scenario in success busy allocation unwind protect suspend context flush helper duplicate duplicate_after case mutex pin image shape short write zero rename readonly image_headers_unreadable image_code_unreadable image_unwind_unreadable image_literal_unreadable image_no_code image_text image_duplicate_names image_unusual_names image_split_code image_cross_section_ambiguity image_unsorted_unwind image_many_sections; do
  "$BUILD/compiled_asi_harness" "$1" "$ROOT/ShutUpAndLetMePlay.asi" "$scenario" "$BUILD/reports"
done
for scenario in npc_success npc_budget npc_choice npc_events npc_stale npc_changed npc_waiting npc_late_change npc_shape npc_allocation npc_unwind npc_protect npc_flush; do
  "$BUILD/dialogue_compiled_harness" "$1" "$ROOT/ShutUpAndLetMePlay.asi" "$scenario" "$BUILD/reports"
done
for scenario in seq_mode0 seq_mode2 seq_no_actor seq_hidden seq_inactive seq_bad_event seq_rejected_type seq_paused seq_mode4 seq_wrong_state seq_shape seq_callback seq_ambiguous; do
  "$BUILD/sequencer_compiled_harness" "$1" "$ROOT/ShutUpAndLetMePlay.asi" "$scenario" "$BUILD/reports"
done
python3 "$ROOT/tests/validate_reports.py" "$BUILD/reports"
