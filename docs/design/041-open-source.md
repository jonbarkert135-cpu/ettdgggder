# 041 — Telemetry policy, updates, open source, reproducibility

**Roadmap items 39–42.** Status: landed and host-tested
(`src_overrides/bedrock/privacy/core/telemetry_policy.*`, `src_overrides/bedrock/updater/*`,
plus four CI gates and four documents).

## 39 — Zero telemetry

Default and shipped state: **nothing is collected.** No analytics, no usage statistics, no
tracking, no fingerprint collection, no history upload, no crash reports.

`TelemetryPolicy` exists so the promise is checkable rather than stated:

- all six categories are enumerated and `Enabled()` answers for each;
- **five of the six are permanently prohibited** — `OptIn()` refuses them. A privacy browser that
  ships an analytics toggle has already decided it might use it one day;
- crash reporting is the only category that may exist, and only as explicit opt-in against the
  **current** disclosure. Consent to an older disclosure version is not consent to this one, and
  `OnDisclosureChanged()` revokes it when the terms change;
- the disclosure says what is sent (including that a crash dump can contain page content), when,
  where, and how to turn it off;
- `Endpoint()` returns empty unless that exact category is opted in **and** the user configured a
  crash service. Bedrock runs none, so "send it anyway" has nowhere to send.

The real enforcement is `scripts/check_no_telemetry.py`: it scans the shipped sources for
histogram macros, metrics services, crash uploaders, variations plumbing and analytics hosts, and
re-checks that `enable_reporting=false`, `safe_browsing_mode=0` and
`use_official_google_api_keys=false` are still in the release GN args. A policy object documents
intent; the scanner is what survives the next contributor who "just wants a counter".

## 40 — Update system with no fixed backend

`UpdateProvider` is abstract. GitHub Releases, a static HTTPS directory, a distribution package
repository and an enterprise share are implementations chosen by configuration. **No hostname is
compiled in** — a test asserts even the status strings contain no domain, because "temporarily"
hardcoding an update host is how a project acquires mandatory infrastructure.

What is *not* configurable, on any provider:

| Refusal | Why |
| --- | --- |
| unsigned manifest | an unsigned release is an unauthenticated one |
| unknown signing key | trust comes from the key that shipped with the browser |
| bad signature | — |
| payload hash mismatch | the manifest is trusted for its claims; the bytes must match them |
| version not newer | a silent downgrade puts the user on a build whose bugs are public |
| non-HTTPS source | including an internal enterprise mirror |

A package-repository install returns `kManagedByOperatingSystem`: the distro already signs and
verifies, and two updaters fighting over one binary is worse than one. An unreachable provider
keeps the current version and says so, instead of treating silence as "up to date".

## 41 — Open source, actually

Item 41 says: do not call the project open source if a critical component cannot be studied or
built. `scripts/check_open_source.py` enforces it — twelve required documents (LICENSE, README,
CONTRIBUTING, SECURITY, BUILD, LICENSING, THIRD_PARTY, THREAT_MODEL, REPRODUCIBILITY, the Chromium
pin, the SBOM, the dependency hashes), each checked for the sections that make it useful, and
eleven critical components that must have source **and a test** in the tree.

New in this batch: `CONTRIBUTING.md` (provenance-first workflow and the full gate table),
`SECURITY.md` (private reporting, honest response targets rather than an invented SLA, scope and
severity), `docs/THREAT_MODEL.md` (Covered/Hardened/Targeted, and an explicit **out of scope**
list — compromised OS, global traffic correlation, logging into your own accounts, extensions you
install).

## 42 — Reproducible builds

`docs/REPRODUCIBILITY.md` states the target, the nine values that make up a release build
manifest, the determinism measures (SOURCE_DATE_EPOCH from the commit, prefix-mapped paths, PGO
off because an out-of-tree profile makes output unverifiable, no build host or user in the
binary) — and a **known gaps** list: upstream Chromium is not fully reproducible everywhere,
archive hashes are incomplete, and nobody has independently rebuilt a release yet, so
reproducibility is *designed for*, not *proven*.

`scripts/generate_sbom.py` generates `build/sbom.json` (CycloneDX 1.5) from `docs/THIRD_PARTY.md`
and `build/dependency-hashes.txt`, and CI runs it with `--check`, so the SBOM cannot drift from
the inventory. `build/dependency-hashes.txt` contains only digests that were actually verified:
placeholder hashes would make the SBOM look complete and be wrong.
