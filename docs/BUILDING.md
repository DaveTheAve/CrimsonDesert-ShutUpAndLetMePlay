# Building and testing

## Build

Requirements: x64 LLVM `clang-cl` and `lld-link`, plus Python 3.10 or newer.
The build uses its own minimal Windows declarations; no game executable or
Windows SDK is needed to compile the ASI.

On Windows run `build.cmd`. On Linux x86-64 run:

```bash
bash build.sh
python3 tools/verify_binary.py ShutUpAndLetMePlay.asi
```

The output is `ShutUpAndLetMePlay.asi`; intermediates stay in `.build/`.
Local validation uses LLVM 17; GitHub-hosted build checks use LLVM 18. The Windows batch script
was not executed on Windows in this validation environment.

## Game-file-free checks

With g++ installed:

```bash
bash tools/ci.sh
```

This checks the source inventory, builds twice, compares the ASIs, verifies PE
metadata, runs Python/tooling tests, compiles five native harnesses and checks
the generated archives. It does not execute the game or publish anything.

## Local native execution tests

Use Linux x86-64, g++, Python and your own exact reference executable:

```bash
bash tests/run_tests.sh "/outside/the/source/CrimsonDesert.exe"
```

The script verifies the executable hash before executing isolated reviewed
routines. It never calls the game's entry point. Reference file version:
**1.0.0.2944**. Reference SHA-256:

`6d348be9d52f81bd35cf7c55e73a5dbfc96cc8268438387c91f7f62c82381fa7`

Do not supply an unknown/untrusted executable. Keep game binaries and captured
logs outside the source tree. The executable is never included in generated archives.

These tests use explicit fixtures and mocks, not Windows or the complete game.
See `VALIDATION.md` for the exact boundaries, especially sequence completion,
choice rendering, audio and gameplay-event recipients.

## Source layout

`Behavior.h`, `Resolver.h` and `VerifiedSignatures.h` implement the normal
cutscene path. `Dialogue.h`, `DialogueResolver.h` and `DialogueSignatures.h`
implement interaction-dialogue offers, bounded progression and native choice
boundaries. `Sequencer.h` and `SequencerSignatures.h` add the distinct native
sequence Skip route and its validation.

`ShutUpAndLetMePlay.cpp` owns startup, the two hook pairs and callbacks.
`Diagnostics.h` formats copied observations; it does not follow game object
pointers from the reporting worker. `Version.h` supplies the mod version.

The sequence extension uses the existing cinematic/input hooks. It does not
add a sequence-update hook, alter the dialogue-advance routine, or directly
call an arbitrary completion method. The original native input handler owns
actor lookup and the sequence Skip dispatch.

## Local packages

```bash
python3 tools/package_release.py
```

This creates player, source, repository, and Nexus-publishing archives with checksums in `dist/`. It has no upload or publication step. Only the ASI from the player
ZIP goes into the game directory.
