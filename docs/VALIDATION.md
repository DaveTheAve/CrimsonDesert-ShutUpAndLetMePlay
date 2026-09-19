# Validation record — 1.0.0

## Release binary

The release is a Windows AMD64 PE32+ ASI. The resource parser verifies file/product version **1.0.0**. Structural checks confirm ASLR/high-entropy addressing, DEP, relocations, unwind data, and the expected KERNEL32-only import set. There are no test exports, CLR/debug directory, writable-plus-executable image sections, or detected private build-path markers.

These are structural checks, not antivirus certification.

## Native isolation tests

The native harness passed **8,085 assertions**, including reproduction of the original Skip suppression, **300 hide/reappear cycles**, hold/cache byte preservation, cache-family sentinels, action/hidden-row guards, changed-signature/name rejection, and comparisons of other cinema modes with unmodified native results.

The compiled ASI harness covers normal startup, busy-prologue retry; allocation, unwind, protection, suspension, context-read and cache-flush failures; helper exclusion, duplicate exclusion, case-insensitive process matching; instance guard, module pin, invalid image and changed-shape rejection; short/failed/zero-byte writes, failed rename, and unwritable output.

Successful simulated gameplay cases verify repeated appearance repairs. Installation-failure cases retain the original native entries and clean resources. Failure to write diagnostics does not disable gameplay.

The game executable itself is not distributed.

## Repository and packaging checks

The game-file-free check builds twice, validates the PE, runs Python tooling regressions, compiles both native harnesses, checks source inventory/action pins, and verifies archives by member contents, CRC, and SHA-256. It does not execute the complete game.

GitHub workflow YAML and relevant scripts are checked locally. Hosted GitHub Actions execution, network package installation, artifact transfer, and the draft-release API are separate environments and are not represented as in-game validation.

## In-game boundary

The release targets supported normal gameplay cinematics on `CrimsonDesert.exe` file version **1.0.0.2944**. No test harness can substitute for the complete Windows game, its renderer, physical input, every scene, every device, every mod combination, or future game updates.
