#!/usr/bin/env bash
# Cross-build Windows x64 ASI using LLVM; no game files or Windows SDK required.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/.build"
for tool in clang-cl lld-link python3; do
  command -v "$tool" >/dev/null || { printf 'Missing build tool: %s\n' "$tool" >&2; exit 1; }
done
mkdir -p "$BUILD"
python3 "$ROOT/tools/make_version_resource.py" "$ROOT/source/Version.h" "$BUILD/version.res"
lld-link /lib "/def:$ROOT/source/kernel32.def" /machine:x64 "/out:$BUILD/kernel32.lib"
clang-cl /nologo /c /O2 /Oi- /GS- /GR- /Zl /W4 /WX /std:c++17   /clang:-fno-builtin "/Fo$BUILD/ShutUpAndLetMePlay.obj" "$ROOT/source/ShutUpAndLetMePlay.cpp"
lld-link /dll /machine:x64 /entry:DllMain /nodefaultlib /noimplib   /dynamicbase /nxcompat /highentropyva /opt:ref /opt:icf /timestamp:0   "/out:$ROOT/ShutUpAndLetMePlay.asi" "/map:$BUILD/ShutUpAndLetMePlay.map"   "$BUILD/ShutUpAndLetMePlay.obj" "$BUILD/kernel32.lib" "$BUILD/version.res"
printf 'Built: %s\n' "$ROOT/ShutUpAndLetMePlay.asi"
