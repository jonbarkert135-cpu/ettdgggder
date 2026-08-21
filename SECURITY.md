# Security Policy

## Reporting a vulnerability

**Do not open a public issue for a security bug.** Use GitHub's private vulnerability reporting on
this repository (Security → Report a vulnerability). If that is unavailable to you, open an issue
containing only "security report, please provide a contact channel" — with no details — and wait
for a response.

Please include: affected version or commit, platform, steps to reproduce, what an attacker gains,
and whether it is already public.

## What to expect

| | |
| --- | --- |
| Acknowledgement | within 5 days |
| Initial assessment | within 14 days |
| Fix or mitigation plan | communicated with the assessment |
| Credit | offered by default, declined on request |

Bedrock is a volunteer-scale project. These are honest targets, not a contractual SLA — a promise
of a 24-hour response would be a nicer sentence and a false one.

## Scope

In scope: anything that breaks a security or privacy property Bedrock claims — sandbox or site
isolation weaknesses introduced by our overlay, a leak past the privacy engine (DNS, WebRTC,
storage partitioning, fingerprint surfaces), the update path (signature or downgrade handling),
the extension permission flow, the password store, or any code path that sends data off the
device.

Out of scope: vulnerabilities in upstream Chromium that we have not modified — report those to the
[Chromium project](https://issues.chromium.org) and tell us the ID so we can track the pin.
Also out of scope: third-party extensions from the catalog (report to their maintainers, and to us
so the entry can be pulled).

## Severity, as we use it

- **Critical** — remote code execution, sandbox escape, or silent installation of an update.
- **High** — a leak that identifies the user across sites, bypass of the update signature check,
  reading the password store while locked.
- **Medium** — a leak past one protection layer that others still cover, a policy bypass that
  needs user interaction.
- **Low** — an inconsistency that misleads the UI without exposing data.

## Supported versions

The current stable release and the current Chromium pin. Bedrock does not backport to older
Chromium bases; upgrading the pin is the security fix.

## Our own security work

- The Chromium security baseline is asserted in a test: 13 required features and 14 forbidden
  switches (`src_overrides/bedrock/privacy/security/security_baseline.*`). No privacy feature may weaken it.
- The update path refuses unsigned releases, unknown keys, hash mismatches, downgrades and
  non-HTTPS sources on every provider (`docs/design/042`).
- The threat model, including what Bedrock explicitly does **not** protect against, is in
  `docs/THREAT_MODEL.md`.
