#!/usr/bin/env bash
# Game-file-free checks used locally and by GitHub Actions. No account access.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
for tool in clang-cl lld-link python3 g++; do
  command -v "$tool" >/dev/null || { printf 'Missing tool: %s\n' "$tool" >&2; exit 1; }
done
if [[ "${GITHUB_ACTIONS:-}" == "true" ]]; then
  python3 tools/check_repository.py --tracked
else
  python3 tools/check_repository.py
fi
bash build.sh
python3 tools/verify_binary.py ShutUpAndLetMePlay.asi
cp ShutUpAndLetMePlay.asi .build/reproducibility.asi
bash build.sh
cmp ShutUpAndLetMePlay.asi .build/reproducibility.asi
python3 -m unittest discover -s tests -p 'test_*.py' -v
# Compile the harnesses without executing game-dependent tests.
CXXFLAGS=(-std=c++17 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-unused-function)
g++ "${CXXFLAGS[@]}" tests/native_harness.cpp -o .build/native_harness
g++ "${CXXFLAGS[@]}" tests/compiled_asi_harness.cpp -o .build/compiled_asi_harness
g++ "${CXXFLAGS[@]}" tests/dialogue_harness.cpp -o .build/dialogue_harness
g++ "${CXXFLAGS[@]}" tests/dialogue_compiled_harness.cpp -o .build/dialogue_compiled_harness
g++ "${CXXFLAGS[@]}" tests/sequencer_compiled_harness.cpp -o .build/sequencer_compiled_harness
python3 tools/package_release.py
python3 - <<'PY'
import hashlib, json, pathlib, re, subprocess
root=pathlib.Path.cwd()
version=re.search(r'#define\s+SULMP_VERSION\s+"([0-9.]+)"', (root/'source/Version.h').read_text()).group(1)
commit=subprocess.run(['git','rev-parse','HEAD'],text=True,capture_output=True)
info={'mod':'ShutUpAndLetMePlay','version':version,
      'source_commit':commit.stdout.strip() if commit.returncode==0 else None,
      'compiler':subprocess.check_output(['clang-cl','--version'],text=True).splitlines()[0],
      'linker':subprocess.check_output(['lld-link','--version'],text=True).splitlines()[0],
      'asi_sha256':hashlib.sha256((root/'ShutUpAndLetMePlay.asi').read_bytes()).hexdigest(),
      'checks':'source allowlist, PE/version validation, repeat-build identity, tooling unit tests, harness compilation, ZIP integrity',
      'native_execution':'not run by this game-file-free check',
      'full_game_execution':'not run'}
(root/'dist/BUILD_INFO.json').write_text(json.dumps(info,indent=2)+'\n')
PY
printf 'PASS game-file-free build checks. Native/full-game execution was intentionally NOT run.\n'
