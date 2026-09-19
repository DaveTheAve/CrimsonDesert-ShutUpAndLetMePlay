# Building and local tests

## Toolchain

The delivered 1.0.0 ASI was built with LLVM clang-cl/lld-link 17. Build scripts
require x64 clang-cl, lld-link and Python 3.10+ on PATH. No Windows SDK, game
file, downloaded dependency during the build or separate C++ runtime is
needed by this mod's scripts. These are developer, not player, requirements.

On Windows, run `build.cmd`. On Linux x86-64:

```bash
bash build.sh
python3 tools/verify_binary.py ShutUpAndLetMePlay.asi
```

The binary is written at repository root; intermediates go to `.build/`.
The Windows batch script is provided but was not executed on Windows here.
`source/Version.h` supplies the built version and Windows resource metadata.
Compiler changes can change the hash; a public CI build is not asserted to
match the supplied LLVM 17 binary byte-for-byte.

## Game-file-free checks

With g++ installed, `bash tools/ci.sh` performs source-inventory checks, builds,
validates PE/metadata, compares two builds, runs Python tooling unit tests,
compiles both native harnesses and validates generated archives. It requires
no game file and performs no game execution. The local version has no account
access; GitHub workflows separately retain artifacts/create requested drafts.

GitHub-hosted jobs select Ubuntu 24.04 and LLVM 18. The workflow commands were
checked locally where possible, but were not executed on hosted GitHub runners
as part of this preparation. Public CI must never fetch or upload game files.

## Optional native tests — local only

On Linux x86-64 with g++, Python 3 and your own exact reference executable:

```bash
bash tests/run_tests.sh "/outside/the/repository/CrimsonDesert.exe"
```

The script checks SHA-256 before executing isolated reviewed native routines
and the compiled ASI against explicit mocks. It never invokes the game's
entry point. It is not a Windows-kernel, loader, renderer or physical-input
test. Do not use an unknown/untrusted executable as its test input.

Reference executable file/product version: **1.0.0.2944**.
PE timestamp: **0x6AABB038**. Image size: **0x17FCD000**.
SHA-256: `6d348be9d52f81bd35cf7c55e73a5dbfc96cc8268438387c91f7f62c82381fa7`.
Full routine fingerprints are in `tests/reference.json`. This is an executable
file version, not a guessed marketing patch number.

Run the local harnesses against your own verified reference executable when you need native execution results. Public CI compiles those harnesses but does not distribute the game file or claim a full-game run. Always record the actual binary hash when testing.

## Source layout

`Behavior.h` owns native UI behavior; `Resolver.h` and `VerifiedSignatures.h`
validate native routines. `ShutUpAndLetMePlay.cpp` owns startup, hook
installation, UI callbacks and the worker. `Diagnostics.h` formats copied
observations on the worker without chasing game-object pointers.

The runtime resolver checks complete masked shapes, cross-references, literal
style names, expected action/widget identity and unwind layouts. It is not a
fixed-address-only patch, but it is not guaranteed to survive future updates.
Keep it fail-closed rather than broadening signatures until unknown builds patch.

## Packages

Run `python3 tools/package_release.py` after building. `dist/` receives the
player ZIP, optional source ZIP, a source-at-root GitHub ZIP, Nexus publishing
kit and external checksums. Explicit inventories exclude build output and
private inputs. Nothing is published by the packaging script.
