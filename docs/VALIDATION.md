# Validation — 2.0.0

## Reference and build identity

The licensed reference executable has file version **1.0.0.2944**, SHA-256:

`6d348be9d52f81bd35cf7c55e73a5dbfc96cc8268438387c91f7f62c82381fa7`

The executable is not distributed. Each locally generated archive includes `CHECKSUMS.md`; `dist/BUILD_INFO.json` records the compiler, source commit, and ASI hash for that build. A local LLVM 17 result is not assumed byte-identical to an LLVM 18 result.

## In-game observations

The maintainer reported successful skipping in NPC dialogue with white-background subtitles, NPC dialogue with black-background subtitles, and normal cutscenes. The accompanying paired diagnostics show an accepted interaction request, native progression and completion activity, sequence-mode-0 prompt repairs and hold forwarding, and a normal cutscene prompt repair, with no recorded guard or write errors.

The capture did not exercise sequence mode 2 or a response-choice stop. Those paths have isolation coverage, not new in-game proof from that session. The counters do not identify subtitle colours. Build verification and in-game observations are separate forms of evidence; neither establishes coverage of every scene or quest outcome.

## Scope

There are three deliberately distinct routes:

| Native control mode | Route |
| --- | --- |
| 1 | Normal-cutscene prompt restoration. |
| 3 | Interaction-dialogue offers, bounded advancement and native choice-wait boundary. |
| 0 and 2 | Additional fixed-row prompt restoration with the original native input/sequence Skip action. |

Native control modes identify distinct routes, not subtitle colours. Runtime counters record which route was exercised; no colour detection is implemented.

## Local results

- Cutscene isolation: **8,085 assertions**, including **300 hide/reappear cycles**.
- Interaction-dialogue isolation: **702 assertions**, including native
  progression, choice-wait and entry-event dispatch under the mocks below.
- Compiled ASI: **47 scenarios** — 21 core/startup/failure cases, 13 interaction
  cases and 13 additional sequence-route cases.
- Paired diagnostics: **44 JSON/log pairs** parsed and cross-checked. Three
  intentional no-output cases do not create a pair.
- Game-file-free Python/tooling checks include signature/mask consistency, packaging, unchanged PNG storage, and publication guards.
- Repeat-build identity, archive CRC/member checksums and an extracted-source
  rebuild were checked.

These are local isolation and tooling results. The separate in-game observations above do not imply every scene or quest was tested.

## Additional sequence route

The new resolver uniquely identifies complete masked native Skip/completion
shapes, agrees their relative references and checks their unwind descriptors.
It also verifies that the existing native input branch calls that Skip route
and that the native callback thunk reaches the expected completion routine.
It does not accept a timestamp or executable-size whitelist.

The extension reuses the two existing hook pairs. It validates the native mode,
state identity, pause flag, fixed-row widgets and visible Fast Forward row.
For hold completion, it additionally checks active UI state, visible Skip
controls and event membership. The original native input handler retains its
own type checks, actor lookup, handle release, fade argument and sequence
Skip dispatch. The mod does not retain a sequence/actor pointer across calls,
change the fast-forward flag, run dialogue advancement on a sequence, or call
an answer-selection function.

The extension independently declines missing, ambiguous or inconsistent
sequence code. The compiled failure tests leave the existing core and
interaction-hook pairs enabled.

The 13 additional compiled scenarios are:

| Scenario | Tested result |
| --- | --- |
| `seq_mode0`, `seq_mode2` | Native fixed-row Skip is restored across 100 hide/reappear cycles per mode. A completed hold reaches the native handler and the mocked sequence endpoint once; native actor-handle release and the fade argument are checked. |
| `seq_no_actor` | The native handler receives the hold but dispatches no sequence action when actor acquisition fails. |
| `seq_hidden`, `seq_inactive`, `seq_bad_event` | Hidden controls, inactive UI and a foreign event are rejected. |
| `seq_rejected_type` | The native input handler's own type check rejects the forwarded event. |
| `seq_paused`, `seq_mode4`, `seq_wrong_state` | No prompt is added for these unsupported/inconsistent states. |
| `seq_shape`, `seq_callback`, `seq_ambiguous` | Altered native code, a mismatched completion thunk and a second signature match disable only the additional sequence route. |

The hide-cycle checks preserve native hiding. They do not require a particular
hidden style where the original game closes the wrapper without changing that
style. Native hold/cache bytes and interaction-context bytes are checked for
preservation. Existing interaction advancement and cutscene tests also pass.

## Interaction route

A native hold notification queues a conversation identity and cursor snapshot.
The natural dialogue update validates that request, stops the current voice
using the linked native method and advances without playing intervening speech.
The native progression routine dispatches entry events and raises its own
response-choice flag. At that flag, the job stops. When no entry remains, the
original update reaches its normal completion call with its original arguments.

Work remains bounded to 64 advancement calls per update, a 16 ms cooperative
budget and a 4,096-call/2-second overall guard. One native call cannot be
interrupted by that budget. Requests expire; unrelated NPC updates do not
consume a foreground request. No temporary context pointer is retained.

## Diagnostic meaning

`npc_dialogue` retains the interaction counters. `sequencer_dialogue` reports
availability, per-mode prompt repairs, guarded native-input forwarding and
immediately observed mode changes. Appearance/input mode histories contain six
buckets: modes 0, 1, 2, 3, 4 and all other values.

A forwarded hold is **not** a verified completed conversation. The no-actor and
rejected-type tests intentionally show forwarding without completion. An
immediate mode change can also differ from a later asynchronous transition.
Neither those counters nor appearance-style checks measure rendered pixels
or confirm quest outcomes. The two files must share a session and revision.

## Test boundaries

The harness executes the compiled ASI and actual native cinematic input,
interaction and appearance routines. Windows services are mocks, not a real
Windows kernel or ASI loader. In sequence-route execution tests, actor
acquisition and the final native sequence Skip endpoint are mocked after the
real resolver has validated their surrounding code. The deeper sequence
completion routine is fingerprinted and statically inspected, **not executed
as a complete quest/sequence in these tests**.

Interaction tests execute the native update, current-entry and progression
routines. Audio stopping, final actor/UI completion, downstream choice rendering
and the final gameplay-event recipient are mocked. Event ordering is tested;
real quest consequences are not. The partial-unwind model does not invoke
Windows `RtlVirtualUnwind`.

No complete Windows game is executed by these harnesses. They do not validate rendered prompts, physical input, sequence-specific choice boundaries, complete quests, store variants, or future updates. The sequence route
does not add the interaction route's explicit stop-at-each-choice loop; the
game's native sequence transition controls what happens next.

## Reference native routes

These RVAs identify the inspected executable for local auditing. They are
not fixed runtime addresses in the mod.

| Route | Reference RVA |
| --- | --- |
| Cinematic input | `0x1063520` |
| Cinematic appearance | `0x10689F0` |
| Interaction dialogue update | `0x5B7B70` |
| Interaction dialogue advance | `0x5B5640` |
| Interaction normal finish | `0x5B6920` |
| Sequence-stage control-mode selection | `0x5EE610` |
| Native sequence Skip | `0xA991D0` |
| Native sequence completion | `0xA994A0` |
