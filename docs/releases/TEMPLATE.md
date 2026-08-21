# Bedrock <version>

<!-- Copy to docs/releases/<version>.md. All six sections are required on every
     channel, including nightly (docs/RELEASES.md). "None" is a valid entry;
     an absent section is not, and scripts/check_releases.py enforces that. -->

**Channel:** stable | beta | nightly
**Released:** YYYY-MM-DD

## Version number

`1.0.0.1`

## Chromium base version

`151.0.7922.173` (commit `a96602f30358e9b5d256a0464e7e4d4bec223004`) — matches
`build/chromium.pin`.

## Security fixes

- Upstream: CVE-YYYY-NNNNN — one line on what it is, and the severity as published upstream.
- Bedrock: what was fixed in overlay code.

None, when that is the truth.

## Privacy changes

Anything that changes what leaves the machine, what is stored, or what a site can observe —
including a changed default. State the direction of the change plainly: a protection that was
weakened is listed here too.

## Dependencies

Third-party components added, removed or version-changed since the previous release on this
channel, with the licence for anything new. Cross-check `docs/THIRD_PARTY.md` and `build/sbom.json`.

## Known issues

What is broken, and the workaround if there is one. This section is never empty on a real release.
