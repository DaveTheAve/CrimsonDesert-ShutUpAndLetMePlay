# Publishing on GitHub and Nexus

## Repository preparation

Suggested repository name: **CrimsonDesert-ShutUpAndLetMePlay**.  
Suggested description: **An actual native cutscene Skip button for Crimson Desert — because fast-forward is a different verb. Windows x64 ASI mod.**

Public repository: **https://github.com/DaveTheAve/CrimsonDesert-ShutUpAndLetMePlay**. Repository contents, including `.github`, `.gitignore`, the source folders, and approved artwork under `assets/`, belong at repository root. Compiled release archives do not.

Review the staged files and run `python3 tools/check_repository.py --tracked` before pushing. Compiled ASIs belong in release downloads, not source history. Never push the game executable or raw diagnostics.

## First release

Use version **1.0.0** and tag **v1.0.0** for this source/binary pair. The supplied player ZIP can be attached directly to a manually prepared GitHub release and to Nexus. Use the **same exact player ZIP** on both sites.

Alternatively, push the version tag and run **Prepare draft release** from the Actions tab, entering the existing tag. It builds the tagged source, verifies the version, and creates a **draft**, never an automatically published release. An existing release for that tag is not overwritten. Publishing remains a maintainer decision.

Build jobs have read-only repository permissions. The separate draft job has the minimum release permissions needed to create the draft and does not check out or execute repository code.

## Nexus page

Use the player ZIP as the main file and optionally the Source ZIP as a separate developer download. The publishing kit is not an installable mod.

Use `release/NEXUS_FIELDS.json` for listing metadata, `release/NEXUS_DESCRIPTION.md` for the description, and `release/PINNED_POST.md` for the support post. The Nexus description file uses simple HTML even though it retains the `.md` filename so there is only one maintained description copy: paste its raw contents into Nexus **Code View** (`</>`), then leave Code View to preview it. Do not paste Markdown syntax into Code View. Apply all required tags listed in `NEXUS_FIELDS.json`.

List a compatible x64 ASI loader as a separate requirement. Select the closest available Gameplay/User Interface category. Do not advertise untested automatic mod-manager installation.

The approved artwork lives in `assets/`:

- `assets/Shut Up and Let Me Play - Header Image.png`
- `assets/Shut Up and Let Me Play - Title Image.png`

Keep those PNGs byte-for-byte as committed: do not resize, recompress, optimize, or convert them. Packaging stores PNG entries without ZIP compression as an extra safeguard against needless processing. For gameplay gallery images, use authentic in-game screenshots of the visible native Skip prompt rather than fabricated gameplay UI.

## Licensing

The project is licensed under the MIT License. Keep `LICENSE` in the repository and source distributions.

## Useful references

- Nexus file submission guidelines: https://help.nexusmods.com/article/28-file-submission-guidelines
- GitHub secure workflow use: https://docs.github.com/en/actions/reference/security/secure-use
- GitHub CLI releases: https://cli.github.com/manual/gh_release_create
- Ultimate ASI Loader: https://github.com/ThirteenAG/Ultimate-ASI-Loader
