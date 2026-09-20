# Publishing on GitHub and Nexus

Repository: https://github.com/DaveTheAve/CrimsonDesert-ShutUpAndLetMePlay

Project description: **An actual native Skip button for Crimson Desert cutscenes and NPC dialogue — because fast-forward is a different verb.**

## Release files

Version: **2.0.0**. Tag: **v2.0.0**.

Use `ShutUpAndLetMePlay-2.0.0.zip` as the player download and `ShutUpAndLetMePlay-2.0.0-Source.zip` as the optional source download. Keep the same exact player ZIP on GitHub and Nexus. The changelog records this version under **2026-09-20**.

`python3 tools/package_release.py` writes checksummed archives in `dist/`. The manually invoked **Prepare draft release** workflow runs from `main`, builds an existing version tag, verifies that it matches `source/Version.h`, and creates a draft for review. It neither automatically publishes nor overwrites an existing release. Build jobs have read-only permissions; the separate draft-creation job does not execute repository source.

Generated release notes contain the matching version's changelog entry, installation links, and validation information. `BUILD_INFO.json` identifies the exact source commit, compiler, linker, and ASI checksum. Review the files and publish the draft through GitHub's release page.

## Nexus page

Use the player ZIP as the main file and the Source ZIP as an optional developer download. The publishing kit is page material, not an installable mod.

`release/NEXUS_DESCRIPTION.txt` contains the approved **BBCode** description. Copy its raw contents into the Nexus BBCode/source editor; do not run it through a Markdown or HTML converter. `release/NEXUS_FIELDS.json` supplies listing metadata and required tags. `release/PINNED_POST.md` supplies support copy.

List a compatible **x64 ASI loader** as a separate requirement. The ASI goes in that loader's supported mod/plugin directory; executable-adjacent setups normally use `bin64`. Do not advertise untested automatic mod-manager installation.

The approved artwork is under `assets/`. The README uses **Shut Up and Let Me Play - Header Image.png**; **Shut Up and Let Me Play - Title Image.png** is also available for the Nexus page. Source and publishing archives retain their exact bytes. Genuine gameplay screenshots, not promotional art, should demonstrate the in-game prompt.

## Repository and license

Commit source, documentation, tooling, tests, and approved artwork—not game files, private logs, or compiled output. Run `python3 tools/check_repository.py --tracked` and inspect the staged files before pushing.

The project uses the MIT License. Include `LICENSE` in distributions.
