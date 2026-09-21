# Changelog

## 2.0.1 — 2026-09-21

- Restores compatibility with executable file version 1.0.0.2949.
- Finds native code by executable-section attributes instead of section names, preserving full signature and cross-reference validation.
- Adds detailed image-validation diagnostics and regression coverage for renamed and multiple executable sections.

## 2.0.0 — 2026-09-20

- Adds manual native hold-to-skip for supported NPC dialogue as well as gameplay cutscenes.
- Handles interaction conversations and sequencer-driven dialogue through their respective native routes, without increasing fast-forward speed.
- Advances interaction dialogue through normal per-entry progression and event dispatch, stopping at the native response-choice boundary without choosing an answer.

## 1.0.0 — 2026-09-19

### Shut Up & Let Me Play: An Actual Skip Button

First public release.

- Restores Crimson Desert's **actual native Skip control** during supported normal gameplay cutscenes.
- Preserves the game's own hold timing, progress, binding selection, controller glyphs, localization, and UI behavior.
- Uses the native cinematic UI instead of a custom overlay.
- Adds process-name filtering and a per-process duplicate-load guard.
- Produces paired diagnostic snapshots with session, process, and revision identifiers.
- Removes live object addresses and full installation paths from public reports.
- Handles short/failed writes, cleans failed temporary files, and retries diagnostics.
- Limits routine diagnostic updates to on-change five-second worker intervals.
- Adds Windows version metadata, reproducible builds, and separate player/source packages.
- Includes public GitHub documentation, repository guards, issue/PR templates, game-file-free build checks, and a manually triggered draft-release workflow.

No automatic skipping, custom overlay, rebinding, hold-duration change, telemetry, updater, subscription tier, or **Skip Button Premium+**.
