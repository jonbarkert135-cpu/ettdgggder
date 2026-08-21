# ADR 0013 — Updates: provider-agnostic transport, security fixes on a clock

**Status:** accepted (2026-08-21) · roadmap items 40, 66, 69, 70, 71 · owner's list: ADR-009
**Documents:** `docs/UPSTREAM_SYNC.md`, `docs/RELEASES.md`, `docs/SUPPLY_CHAIN.md`

## Context

A browser's update mechanism is its most security-critical network path: it is the one channel
allowed to replace the binary. It is also, in Chromium, tied to Google infrastructure that a
project claiming zero telemetry cannot use unexamined — an update check is a periodic ping
carrying a machine identifier and a version.

And it sits on a deadline, because being derived from Chromium means inheriting Chromium's
vulnerabilities on Chromium's disclosure schedule.

## Decision

1. **Provider-agnostic updates.** The updater speaks to an endpoint described in configuration:
   Bedrock's own release server, a distribution mirror, a corporate one, or none at all for
   package-managed installs. No Google update infrastructure, no hardcoded vendor.
2. **The check carries the minimum.** Channel, version, platform. No installation identifier, no
   usage counters, no first-run beacon. It is a request for a file list, not a report.
3. **Security fixes are on a clock**, measured from the public upstream fix:
   critical **72 h**, high **7 days**, medium **14 days**, low **30 days**. The policy is in code
   (`updater/release_policy`), not only in prose, and a release that misses a deadline is a
   release-blocking failure rather than a discussion.
4. **A security update outranks everything.** Any pending feature work, any soak period, any
   channel promotion rule yields to it — encoded in `release_policy` so it cannot be argued away
   under time pressure.
5. **Everything is verified before it is applied**: Ed25519 signature, SHA-256 digest, and the
   SBOM and provenance chain of `docs/SUPPLY_CHAIN.md`. `scripts/verify_release.py` is the same
   check CI runs.

## Alternatives considered

* **Reuse Chromium's Omaha/component updater.** Mature, and it reintroduces exactly the reporting
  item 39 removed. Rejected.
* **Rely on distribution packaging only.** Good on Linux, unacceptable on Windows, where the user
  would be left on a vulnerable build.

## Consequences

* Running Bedrock's release infrastructure is an operational commitment; if it lapses, users must
  still be able to verify and install a release by hand, which is why the verification tooling is
  in the repository.
* The three channels (nightly / beta / stable) each carry the six mandatory note fields of item
  71, so "what changed" is answerable per build.
