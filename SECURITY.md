# Security and sensitive reports

For ordinary mod bugs, use the bug-report form. Include only the requested
current diagnostics and relevant environment details, not game binaries,
private saves, account data, credentials or full process dumps.

For a security-sensitive problem, use GitHub's private vulnerability-reporting
feature when the maintainer has enabled it. Otherwise open a minimal issue
asking for a private contact route, without posting sensitive details publicly.
No private contact address or response-time commitment is invented here.

The ASI changes two native entry points in the running game after validation.
It does not implement network requests, an updater or executable-on-disk
patching. Those facts do not make an unsigned native plugin risk-free. Verify
where binaries came from and inspect source/checksums; do not disable security
software just to install the mod. Structural checks are not malware certification.

The supplied workflows use pinned actions and read-only build jobs. Only the
separate manual draft-creation job has release-write permission; it does not
check out or execute repository code. Review workflow/dependency changes.
Do not add a self-hosted runner containing game files/secrets for public PRs.
