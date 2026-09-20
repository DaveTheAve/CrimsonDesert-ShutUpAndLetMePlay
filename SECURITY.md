# Security and sensitive reports

For ordinary mod bugs, include the two current diagnostic files from the same run, game/mod/loader versions, input device, relevant other mods, and the affected cutscene or conversation. Do not post game binaries, private saves, account data, credentials, or full process dumps.

For sensitive issues, use GitHub's private vulnerability reporting when enabled. Otherwise request a private contact route without posting sensitive details publicly.

The unsigned ASI changes verified native entry points in the running game. It does not patch the executable on disk, perform network requests, self-update, or choose conversation responses. Native plugins can still crash the game or affect scripted behavior. Structural checks and checksums are not antivirus certification or a guarantee of every quest outcome. Keep normal save backups; do not disable security software to install the mod.

The build workflow has read-only repository permissions. The separate, manually invoked draft-release workflow is restricted to `main`; only its draft-creation job has release-write permission and it does not execute repository source. Review workflow changes. Do not attach public PRs to a self-hosted runner containing game files or secrets.
