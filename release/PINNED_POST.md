# Before posting “it doesn't work”

A beloved internet tradition, but first:

1. Make sure `ShutUpAndLetMePlay.asi` is in your loader's mod/plugin directory (`bin64` for executable-adjacent ASI setups) and that a compatible x64 ASI loader is actually loading it.
2. Keep **one** copy in loader-scanned folders. A file renamed to `*_backup.asi` is still loadable, because computers refuse to appreciate our intentions.
3. This is manual hold-to-skip for supported cutscenes and NPC dialogue, not auto-skip. Hold the native Skip control when it appears.

For a bug report, include the game/mod/loader versions, cutscene or NPC conversation, input device, relevant other mods, and **both current diagnostics from the same run**:

- `ShutUpAndLetMePlay.log`
- `ShutUpAndLetMePlay_UpdateReport.json`

The JSON separates `runtime`, `npc_dialogue`, and `sequencer_dialogue`. Check the dialogue sections' `enabled` flags and reasons independently.

If the report says `disabled`, include the reason. After a game update, please do not defeat the compatibility checks in the name of science; mystery machine code has enough confidence already.

Do not upload the game executable, saves, or account information.

And yes: it is supposed to actually skip. That is the entire joke.
