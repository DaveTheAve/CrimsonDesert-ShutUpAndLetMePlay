# Validation — 2.0.3

## Offline candidate

Build ID: **`2.0.3-update-resilience-r1`**.
Target executable file version: **1.0.0.3663**.
SHA-256: `8be67178d9b9a149c361a731051665e0ba4d441e250ae1dfe645bd6e524345c8`.
PE timestamp: `0x6AF5D198`; image size: `0x144E9000`.

The game executable is a local test input only and is not distributed. The complete Windows game was not launched. This candidate needs in-game confirmation. No GitHub commit, push, tag or release is part of this offline work.

## Compatibility break

The modern cinematic Interaction, Appearance and keyguide binding/cache functions retain their previous validated shapes. The input handler changed outside its native Skip branch; its Skip branch moved, and several interaction-dialogue functions and sequence internals changed. Version 2.0.2 required a complete input-handler match during core startup, so that unrelated change prevented otherwise compatible cutscene hooks from installing.

The 3663 dialogue cache has a different native getter and loaded-record layout. Its getter has three chained unwind fragments rather than one contiguous function record. The dialogue-update entry also changed: copying 15 bytes would split its final `push r12` instruction. The new trampoline copies all 16 bytes through that instruction and uses the corresponding unwind record. The game still owns hold duration, bindings, UI rendering and native transitions.

## Update-resilience changes

**Cutscene validation is scoped to the relevant native action.** The exact 52-byte Skip block is uniquely found across executable sections. Both native guide comparisons, event phase, the call to validated Appearance, and branch destinations are checked. All exit jumps must agree with the containing native function's exact epilogue. This does not use a guessed address, a first-match fallback, a similarity threshold, or a blanket executable-version allowance.

**Shared input and interaction advancement are separate dependencies.** Hooking input additionally requires the verified prologue, native event/type/member/active-state checks, bounded branch targets, common game-manager reference and full native unwind descriptor. An unavailable interaction update/cache does not disable separately validated sequencer input. Invalid optional routes do not remove otherwise validated core cutscene hooks. An invalid core/UI contract still disables core support rather than patching unknown code.

**Small keyguide-layout shifts can be rediscovered.** Only the six action/family/subtype displacement operands in the complete setter and reader shapes may vary. The two methods must independently agree; fields must be aligned and in bounds, and family/subtype must follow the action pointer by the expected distances. Their call chain, action identities, fixed instructions, function boundaries, unwind data and runtime widget/style guards are still validated. A synthetic coordinated 16-byte field shift is exercised through the actual native setter/reader and compiled ASI; disagreement and invalid bounds are rejected.

**Changed interaction and sequence functions use reviewed exact patterns.** The new cache reader observes only already-loaded records; it does not invoke the native lazy loader from UI callbacks. The three cache-getter unwind fragments must agree with their parent. Interaction requests remain bounded, retain per-entry dispatch, expire on stale state, and stop at the native response-choice flag without selecting an answer. Sequence completion remains the game's native route, not the interaction advancement loop.

These changes address specific avoidable dependencies and provide tested graceful degradation. They do not make the mod update-proof: new code generation, changed input semantics, widget types, state layouts, or hook ABIs can still require an update.

## Execution results

| Executable reference | Compiled-ASI scenarios | Parsed matching JSON/log pairs |
| --- | ---: | ---: |
| 1.0.0.3663 | 46 | 46 |
| 1.0.0.3479 | 46 | 46 |
| 1.0.0.2949 | 59 | 56 |
| 1.0.0.2944 | 59 | 56 |

Total: **210 compiled-ASI scenarios and 204 matched diagnostic pairs**. The three intentional no-output cases in each old-reference suite do not create reports. Exact executable hashes are verified before the native harness runs.

On both modern references, keyguide fixtures invoke the native binding setter and cache reader; they do not create action fields by copying the mod's assumed offsets. The core success case covers 51 activation/appearance repairs, 50 hide/reappear cycles, native deactivation, and hold/cache preservation. The native completed-hold path is exercised for cutscenes and for sequence modes 0 and 2, with final actor/sequence effects mocked.

Modern interaction tests execute native cache/current-entry/update/advance routines, normal completion dispatch, three ordered per-entry events, a response-choice stop, stale/changed requests, an existing-choice guard and 100 dialogue entries across seven bounded updates. Empty, excessive-count and unreadable cache fixtures are rejected. The native choice-wait instruction executes; downstream choice UI rendering is bypassed by a test-only jump to the native journal path. The production ASI does not perform that test-only patch.

Fault tests mutate unrelated input bytes, interaction-update code, sequence code and the shared input guard independently. They verify the intended remaining hook count and route availability. A corrupted input guard permits only core prompt testing; that deliberately corrupted native input is not executed. Changed/ambiguous UI or keyguide code, wrong bindings, locked styles, allocation and unwind-registration failures remain rejection cases.

Each older-reference suite additionally passes **8,087 cutscene assertions**, including 300 hide/reappear cycles, and **704 interaction-dialogue assertions**.

## Build, tooling and packaging

- **56 game-file-free Python/tooling tests**.
- Synthetic PE/scanner coverage: **24,095 assertions**, including 12,000 comparisons with an exhaustive masked-scan oracle.
- PE version/import/section validation; two matching builds with the same compiler; six native harnesses compiled with warnings treated as errors.
- Source allowlist, archive contents/CRC/SHA-256 checks and unchanged PNG byte/storage checks.
- Independent source-archive extraction and rebuild reproduced the delivered ASI byte-for-byte.

Delivered ASI: **200,704 bytes**, SHA-256 `9eb1fcd3395e62e547a96194b0b139cda8c0572bf128b3abaea23fcda02759b6`.

Local compilation uses LLVM 17; a different compiler version is not assumed to produce an identical binary. `BUILD_INFO.json` and the delivered checksum manifest record the actual build. No hosted GitHub workflow was triggered for this offline candidate.

## Unwind and test boundaries

The input trampoline saves RBX then pushes four nonvolatile registers. The new dialogue-update trampoline also pushes four registers, but its RBX home slot and copied length differ. The partial-prologue unit model checks every instruction offset and a body offset, using a fixed establisher-frame base for SAVE operations and adjusting RSP only for completed PUSH operations. RBX saves are described at the completed copied frame; before that point RBX remains unmodified. This models the documented unwind rules, not an actual Windows kernel unwind.

Reference semantics: Microsoft x64 exception handling documentation and the .NET AMD64 unwinder implementation. No live Windows loader, kernel, renderer, physical input device or audio system is exercised by these Linux tests. Actor acquisition/handle release, voice stopping, final conversation/sequence completion and quest-event recipients use explicit mocks. Tests verify dispatch and event ordering, not complete quest consequences or rendered pixels. Sequence-specific response choices remain controlled by the game's native sequence logic.

The new target has not yet been confirmed in game. Previous user confirmation of the 2.0.2 keyguide fix applies to file version 1.0.0.3479, not this new candidate. Epic-store runtime compatibility is not claimed without its actual executable/run.

## Diagnostics

Both files identify build **`2.0.3-update-resilience-r1`**. In addition to existing per-route counters, the resolver reports the native input owner/length, Skip helper, shared-input validation and installation status, selected dialogue cache layout, and copied dialogue-update entry length. `enabled` describes that route, not every route in the mod.

The two files must agree on build ID, session and revision. Native style checks and forwarded holds are not proof of visible prompts or completed quests. No live object addresses or private installation paths are added to public diagnostics.
