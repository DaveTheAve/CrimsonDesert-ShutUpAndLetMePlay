# Contributing

Keep this mod focused on native, manually requested skipping of supported gameplay cutscenes and NPC dialogue. Preserve native hold timing, binding/glyph selection, localization, and UI behavior.

Use focused changes with an observable problem, the relevant native route, and regression coverage. Keep the routes distinct: normal cutscenes, interaction-dialogue progression, and native sequencer Skip are not interchangeable.

Interaction-dialogue changes must respect the native response-choice flag and per-entry event ordering. Never select responses, set quest flags, or retain a temporary dialogue context pointer across updates. Sequencer dialogue stays in its native input/sequence route rather than borrowing the interaction-advance loop.

Run `bash tools/ci.sh` for game-file-free checks. Native behavior, resolver patterns, ABI declarations, and hook-installation changes also need the local reference tests in `docs/BUILDING.md`, plus an identified in-game check where possible. Distinguish what was built, what ran under mocks, and what was observed in game.

Do not commit game executables, extracted game assets, disassembly dumps, private diagnostic captures, saves, credentials, loader DLLs, or compiled output. Keep local test inputs outside the repository. The approved promotional PNGs under `assets/` are part of the repository.

Before pushing:

```bash
git status --short
python3 tools/check_repository.py --tracked
```

Review the actual staged files. Ignore rules cannot remove an already-pushed secret.
