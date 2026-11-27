# Validation — 2.0.4

## Offline candidate

Build ID: **`2.0.4-build3840-r2`**.
Target executable file version: **1.0.0.3840** (Steam PC patch of 2026-11-26).
SHA-256: `ea2d84d947521332b002359818960ffd97765f37b447503d8a1efad4040269c8`.
PE timestamp: `0x6B07E5E9`; image size: `0x13D05000`.

The game executable is a local test input only and is not distributed. The automated suites below did not launch the complete Windows game.

**In-game check (user-reported, 2026-11-27, Steam PC, 1.0.0.3840):** build `2.0.4-build3840-r2` worked for gameplay cutscenes, sequence-driven dialogue and interaction NPC dialogue. This is the player's own report; no diagnostic capture from that session was reviewed. Epic-store runtime compatibility is not claimed.

## What 2.0.3 rejected

Against 1.0.0.3840, 2.0.3 reported `updated native cinematic/input shape missing or ambiguous`: Interaction, the keyguide style setter, binding and cache methods still matched, but the cinematic Appearance pattern did not.

## Contract changes in 1.0.0.3840

**Cinematic Appearance: unchanged code, defective mask.** The 3840 function is instruction-identical to the reviewed shape. The `vmovss xmm0,[rip+disp32]` operands at +0xD6 and +0x155 had their mask shifted by one byte: the fixed ModRM `0x05` was masked while the displacement's high byte was fixed (`0x04`). The duration constant moved far enough that this byte is now `0x05`. The same shifted mask existed on 12 four-byte-prefix RIP operands (VEX `vmovss`/`vandps`, REX `cmovne`) in `New3479Signatures.h`. Each now fixes its opcode/ModRM bytes and masks exactly its 32-bit displacement. No other byte became wildcarded.

**Input, Skip branch, Interaction, keyguides: compatible.** The input owner (2788 bytes), its verified prologue, event/type/member guards, native Skip block, common 27-byte epilogue and unwind descriptor match the 3663 contract. The keyguide setter and reader independently agree on action/family/subtype fields `0x1E8/0x1F0/0x1F1`. The game-state mode byte `0x10B6` and pause/active bytes `0x10B5/0x10B7` remain fixed bytes in validated native code.

**Sequencer: private object fields moved.** The sequence Skip (705 bytes) and completion (808 bytes) routines keep the 3663 instruction stream. Only private sequence-object displacements changed: state byte `+0x28E -> +0x29B`, step counter `+0x2A4 -> +0x2B0`, and an owner field `+0x1048 -> +0x1040`. The mod never reads or writes these fields. `New3840Signatures.h` adds an exact reviewed 3840 profile with the unchanged 3663 masks. A match from more than one profile is rejected as ambiguous. The actor helper, its input call, state facade, completion thunk and unwind records are still cross-checked.

**Interaction dialogue: new rule-cache ABI, ported as cache layout 2.** The rule-cache getter is no longer `getter(&id)`. It is `getter(store,id)`, a thread-affine, lock-guarded lazy loader: the owner check passes when `[owner+0x19]` is set or the current thread is `[owner+0x1C]`. Entries are 48 bytes at `store+0x18`, the count is at `+0x20`, and a loaded database is at `entry+0x20`. Callers substitute a fallback record for a null result. Current, Finish and Advance must agree on the same getter, store global and fallback global, and Advance must call the verified Current routine. The getter shape and its unwind record are exact. The mod's reader observes only an already-loaded `entry+0x20` from the dialogue update thread and never calls the loader; an unloaded entry stays ineligible. The record row layout (`+0x110`, 0x90 stride), context fields and the choice-wait byte `+0x5D` are unchanged and are exercised by the native routines below.

The 3840 update (2539 bytes) restored the 3479 entry: `mov rax,rsp` followed by three home-slot stores. Its first 15 bytes are identical to 3479, contain no RIP operand, move no stack pointer and modify no nonvolatile register, so the reviewed 15-byte trampoline with an empty UNWIND_INFO is reused. The resolver checks that byte equality. Update, Advance, Current, Finish and getter are exact reviewed shapes, generated with the 3663 masking convention; the generator reproduces all seven 3663 masks byte for byte. The update's world, stop-voice thunk, invalid-voice-key and finished-result operands, and its Current/Advance/Finish calls, use the 3479 operand offsets. Every native unwind record is compared.

Old builds keep their existing paths. 3479/3663 sequencer selection is unchanged, and the 3840 shape cannot match them because its moved field bytes are fixed. The corrected masks match the original 3479 bytes, so the same instructions still match. Only the displacement high byte is now free.

## Execution results

| Executable reference | Compiled-ASI scenarios | Parsed matching JSON/log pairs |
| --- | ---: | ---: |
| 1.0.0.3840 | 49 | 49 |

The 3840 suite executed **432,088 assertions**. It runs all 46 modern scenarios plus three 3840-specific faults: a changed cache getter, a caller whose store operand disagrees (both disable only interaction dialogue: 3 hooks), and a second exact sequence Skip shape (sequencer disabled, interaction dialogue retained).

Interaction tests execute the native 3840 Current, Advance, Update and cache getter, including its owner-thread check. They cover normal completion dispatch, three ordered per-entry events, a response-choice stop after the native `+0x5D` write, stale/changed/waiting requests, and 100 dialogue entries across seven bounded updates. Empty, excessive-count and unreadable cache fixtures are rejected. As before, choice rendering after the native wait-flag write is bypassed by a test-only jump to the native journal path (Advance+0x32C to +0x11C7). The production ASI does not perform that patch. External lookups and event dispatch are mocked at the call sites that correspond to the 3663 mocks.

The core cutscene case covers 51 activation/appearance repairs, 50 hide/reappear cycles, native deactivation, and hold/cache preservation through the executable's native binding setter and cache reader. Sequence modes 0 and 2 run through the native input handler and actor helper; no-actor, hidden, inactive, rejected-type, paused and wrong-state routes are also covered.

The 1.0.0.3663, 3479, 2949 and 2944 native suites were **not re-run** for this candidate: those executables are not available locally. The game-file-free unit tests and harness compilation still cover their code paths. Re-run `tests/run_tests.sh` against each listed reference before publication.

## Build, tooling and packaging

- **60 game-file-free Python/tooling tests**; source allowlist; PE version/import/section validation; two matching builds; six native harnesses compiled with warnings treated as errors; archive contents/CRC/SHA-256 checks.
- Toolchain: Ubuntu clang/LLD **17.0.6**, in a Linux x86-64 container. With this toolchain the unmodified 2.0.3 source builds to 182,272 bytes, rather than the published 200,704-byte binary, so ASI hashes are only comparable within one toolchain.

Delivered ASI: **204,288 bytes**, SHA-256 `4502dae8337a3b1ea0f5fb6f7b3abf3981318d356065beb6498be28f0e8b0b6e`.

## Unwind and test boundaries

The 3840 cutscene and shared-input trampolines use the same validated entry bytes and unwind records as 3663; the 3840 dialogue-update trampoline uses the 3479 15-byte entry and empty UNWIND_INFO. The rules from 2.0.3 still apply: the partial-prologue model follows the documented Windows unwind rules, but no actual `RtlVirtualUnwind` call runs. Actor acquisition/release, final sequence completion, rendering, audio and quest recipients are mocks. The tests verify dispatch and ordering, not rendered pixels.

## Diagnostics

Both files identify build **`2.0.4-build3840-r1`**. On 3840, expect `status: active`, `native_profile: 1`, `input_hook_enabled: 1`, `npc_dialogue.enabled: 1`, `dialogue_cache_layout: 2`, `dialogue_update_stolen_bytes: 15`, `sequencer_dialogue.enabled: 1`, and keyguide offsets `488/496/497`.
