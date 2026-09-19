# Contributing

Keep this mod narrowly focused on the native normal-gameplay cutscene Skip
control. Preserve the working native action, hold duration, binding/glyph
selection and UI behavior unless an intentional change is discussed first.

Use a focused issue/PR to explain the observable problem, relevant code path,
smallest fix and regression coverage. Avoid unrelated refactors of the known-
good hook path. Do not replace the native UI with an overlay or use generic
Show calls on unverified child-selector handles.

Run `bash tools/ci.sh` for game-file-free checks. Changes to native behavior,
resolver patterns, ABI declarations or hook installation also need the local
reference tests described in `docs/BUILDING.md`, plus a clearly identified
in-game check where possible. Report what was and was not executed.

Never commit the game executable, extracted game assets, full disassembly
listings, private logs, saves, credentials, third-party loader DLLs or compiled
build output. Keep local test inputs outside the repository. Before pushing:

```bash
git status --short
python3 tools/check_repository.py --tracked
```

The check requires staged/tracked files. Ignore rules are not an enforcement
boundary: they can be overridden and cannot remove an already-pushed secret.
Review the actual staged files.
