![Shut Up & Let Me Play - An Actual Skip Button](assets/Shut%20Up%20and%20Let%20Me%20Play%20-%20Header%20Image.png)

# Shut Up & Let Me Play
## An Actual Skip Button for Crimson Desert

> **Other “skip” mods:** *What if the cutscene happened at ludicrous speed?*  
> **This mod:** *What if the cutscene stopped happening?*

A tiny native mod for **Crimson Desert** that restores the game's own **Skip** control during supported gameplay cutscenes and NPC dialogue.

Hold the displayed Skip button and the cutscene or dialogue actually ends!

Not faster. Not *aggressively progressing toward the end*. Not “technically you watched the whole thing, but in the time it takes to sneeze.”

**Skipped.**

We have finally achieved the technology promised by the word *skip*.

---

## The revolutionary feature list

- **An actual Skip button.** We spared no expense.
- **NPC dialogue too.** Supported conversations can now end before you start mentally composing your shopping list.
- Uses **Crimson Desert's native cinematic UI** rather than drawing a fake prompt on top.
- Uses the **game's own hold-to-skip behavior**, progress, bindings, controller glyphs, localization, and UI scaling.
- Leaves the game's normal cinematic hide/show behavior alone outside the targeted path.
- Does **not** auto-skip every cutscene like an impatient housecat standing on your keyboard.
- Does **not** remap your controls.
- Does **not** add telemetry, an updater, an account, a newsletter, a battle pass, or a blockchain.
- Does **not** “skip” by making the cutscene run at 8x/16x/whatever-the-camera-can-survive.

No disrespect to fast-forward mods. Fast-forward is useful. It is simply a different verb.

---

## Installation

Players should download the **latest release** from [Releases](https://github.com/DaveTheAve/CrimsonDesert-ShutUpAndLetMePlay/releases/latest). GitHub's automatically generated “Source code” archives are source code, which is famously bad at being an installed ASI.

1. **Completely close Crimson Desert.** Yes, actually close it. The game cannot politely replace code it is currently using.
2. Keep your existing working **x64 ASI loader**. If you do not have one, install a compatible loader first. [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) is one option. The loader is **not bundled** with this mod.
3. Place **`ShutUpAndLetMePlay.asi`** in the mod/plugin directory used by your compatible **x64 ASI loader**. Follow that loader's directory conventions; setups that load ASIs beside **`CrimsonDesert.exe`** normally use `bin64`.
4. Launch the game normally.
5. Enter a supported gameplay cutscene, hold the native Skip button, and enjoy the breathtaking cinematic experience of **not being in the cinematic anymore**.

Only the ASI is installed. Do **not** replace `CrimsonDesert.exe`.

Keep only one copy of the mod in directories scanned by your ASI loader. Backups belong somewhere the loader cannot see them, because a file named `ShutUpAndLetMePlay_backup_totally_not_a_mod.asi` is, tragically, still an `.asi`.

### Uninstall

Close the game and remove **`ShutUpAndLetMePlay.asi`**. You may also delete:

- `ShutUpAndLetMePlay.log`
- `ShutUpAndLetMePlay_UpdateReport.json`

Leave your ASI loader installed if other mods still use it.

---

## How it works, without pretending magic happened

Crimson Desert already contains native Skip behavior and native Skip controls. This mod restores the relevant controls and uses the appropriate native route for the active cutscene or conversation.

**Normal cutscenes:** the game retains its native Skip action. The mod makes the existing control available when the normal cinematic row appears.

**Interaction dialogue:** holding Skip stops the current voice and advances through the game's dialogue progression and per-entry event paths without playing the intervening speech. It stops when the game raises its response-choice flag. Choose your own response, then hold Skip again for the next section. When no entry remains, the normal dialogue update completes the conversation.

**Sequencer-driven dialogue:** holding Skip goes through the game's native input handler and sequence Skip action. The game owns the transition and completion; this route does not run the interaction-dialogue loop or automatically choose a response. It does not add an extra stop-at-every-choice rule to the native sequence logic.

The game still owns hold timing, bindings, glyphs, localization, UI scaling, and rendering. There is no custom overlay, fake progress ring, forced controller mode, binding rewrite, or executable-on-disk patch.

In other words, the mod is not inventing a new Skip button. It is standing behind the game's existing Skip button and whispering:

**“You can come out now.”**

---

## Compatibility

### Built to survive game updates

**Shut Up & Let Me Play does not rely on fixed memory addresses or a fixed executable layout.** At startup, it pattern-scans `CrimsonDesert.exe` for the native functions it needs, resolves their current locations, and verifies the surrounding code before installing anything. Executable sections are identified by their attributes, not names that can change between builds.

That means many game updates that simply move code around, change RVAs, or rearrange the executable should require **no mod update at all**. The mod finds the functions again wherever the update moved them and gets back to the important business of letting you leave the cutscene.

If Pearl Abyss substantially rewrites the relevant native code, a signature becomes ambiguous, or another mod has already modified the same entry points, Shut Up & Let Me Play fails safely instead of hooking an unknown function and hoping for the best. At that point, the mod may need an update.

The currently tested **`CrimsonDesert.exe` file version is `1.0.0.2949`**. That version number records what has been tested; it is **not** a hard-coded compatibility lock.

Do not hot-load or hot-unload the ASI.

---

## FAQ

### “Does it really skip the cutscene?”

Yes. That is why the subtitle is **An Actual Skip Button** and not **A Surprisingly Motivated Fast-Forward Button**.

### “Does it automatically skip every cutscene?”

No. You still decide when to use it. Some people enjoy stories. We support their unusual lifestyle choices.

### “Why do I have to hold the button?”

Because that is Crimson Desert's native behavior. The mod restores the game's existing interaction rather than replacing it with a custom instant-skip system.

### “Will it work after every game update forever?”

If I possessed that level of clairvoyance, this repository would contain lottery numbers instead of signature validation. What the mod *can* do is rediscover the native functions at runtime instead of relying on fixed addresses, so ordinary code relocation should not bother it. If the functions themselves materially change, it fails closed until the signatures can be updated.

### “Why not just use a fast-forward mod?”

You absolutely can. They solve a related problem. This project exists for the deeply unreasonable crowd who interpreted **Skip** to mean **Skip**.

### “Is there a config file?”

No. The current feature set has one major philosophical position: when you ask to leave the cutscene, you should leave the cutscene.

---

## Diagnostics & bug reports

The mod writes two small diagnostic files beside the installed ASI:

- **`ShutUpAndLetMePlay.log`**
- **`ShutUpAndLetMePlay_UpdateReport.json`**

If something breaks, use the repository's bug-report form and include both files from the same run, plus:

- game version / executable file version
- mod version
- ASI loader
- input device
- the scene or NPC conversation where it happened, including any response choice
- relevant other mods

The `image_validation` section records the startup validation stage, section layout, and the specific reason for an image rejection. The JSON separates normal cutscene activity (`runtime`), interaction dialogue (`npc_dialogue`), and sequencer-driven dialogue (`sequencer_dialogue`). Check each dialogue section's `enabled` flag and reason separately. Choice stops, progression, and per-mode activity help identify which route was used.

`active` means the core cutscene hooks installed successfully. It does **not** mean the diagnostic system grew eyes and personally watched your pixels.

`disabled` includes a reason. If the report says `active_warning`, exit the game and remove the ASI before trying again.

Please do **not** upload `CrimsonDesert.exe`, save files, or account information. The game executable is very large and, more importantly, not a bug report attachment.

---

## Building from source

Building requires:

- LLVM `clang-cl` / `lld-link` for x64
- Python 3.10+

Use `build.cmd` on Windows or `bash build.sh` for the Linux cross-build environment used by the project.

See [docs/BUILDING.md](docs/BUILDING.md) for the serious instructions and [docs/VALIDATION.md](docs/VALIDATION.md) for what was actually tested versus what merely looked persuasive in a terminal window.

GitHub Actions builds without game files, runs release-tool tests, compiles the native harnesses, and verifies package integrity. Public CI does **not** execute the game or magically simulate every future Pearl Abyss update.

---

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before changing the native hooks and [SECURITY.md](SECURITY.md) before reporting a sensitive issue.

The GitHub repository is:

**`CrimsonDesert-ShutUpAndLetMePlay`**

Because `ShutUpAndLetMePlay` alone sounds like a repository for either a game mod or a very specific family disagreement.

Publishing instructions live in [docs/PUBLISHING.md](docs/PUBLISHING.md). Nexus-ready description/support copy is under `release/`.

---

## License

Licensed under the **MIT License**. See [LICENSE](LICENSE).

---

## In summary

**Fast-forward:** the cutscene is still happening, but everyone suddenly drank six espressos.  
**Shut Up & Let Me Play:** the cutscene is over.

Hold Skip. Resume game. Enjoy your time. #VivaLaSkip
