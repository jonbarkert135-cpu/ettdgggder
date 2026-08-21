# Bedrock Threat Model

A browser that does not say what it protects against is asking to be trusted for things it cannot
do. This document is the answer, and it is deliberately as specific about the failures as the
defences.

## Levels

Bedrock uses a three-level model (the concept is shared with several privacy projects; the wording
here is ours):

| Level | Adversary | What Bedrock does |
| --- | --- | --- |
| **Covered** | Advertising networks, data brokers, ordinary cross-site tracking | Default configuration: content and tracker blocking, storage partitioning, third-party cookies blocked, fingerprint normalisation level 1, HTTPS upgrade, tracking-parameter stripping |
| **Hardened** | Aggressive fingerprinting, correlation across sessions, network observers | Stricter fingerprint levels, encrypted DNS, HTTPS-only, per-profile separation, private windows, cookie lifetimes |
| **Targeted** | A specific adversary with resources who is trying to identify *you* | Tor transport mode with circuit isolation, temporary profiles, New Identity — and the honest statement that a browser is one component of a threat model, not the whole of it |

## In scope

1. **Cross-site tracking** — third-party requests, cookies, storage, cache and network-level
   identifiers. Handled by one decision point (`BlockingPipeline`) plus storage partitioning.
2. **Fingerprinting** — 21 documented surfaces, normalisation first and perturbation only where
   normalisation is impossible, deterministic per (session, site, surface) so that a script cannot
   average out the noise.
3. **Passive network observation** — HTTPS upgrade and HTTPS-only, encrypted DNS, no plaintext
   fallback in strict mode.
4. **Local data exposure** — per-profile isolation with nothing shared, an encrypted password
   store that stays locked, private windows that leave no history or download record.
5. **Malicious or over-permissioned extensions** — permissions disclosed from the manifest,
   updates that widen permissions staged for review, extensions off in private windows by default.
6. **A hostile update path** — signatures, trusted keys, payload hashes, no downgrades, HTTPS only,
   on every provider.
7. **Us** — Bedrock operates no server. There is no telemetry, no account, no sync, and
   `scripts/check_no_telemetry.py` fails the build if reporting machinery appears.

## Explicitly out of scope

Saying this plainly is part of the model:

- **A compromised operating system.** Malware with your privileges, a keylogger, or a hostile
  kernel defeats any browser. Nothing here helps.
- **A compromised or coerced device you unlock.** Bedrock protects data at rest only as far as the
  platform keystore does.
- **Global traffic correlation.** An adversary who can watch both ends of a Tor circuit is outside
  what any browser can address.
- **Deanonymisation by what you type.** Logging into an account identifies you regardless of
  fingerprint protection. Bedrock cannot unsay that.
- **Sites you allow.** An exception you grant is a decision the browser will respect.
- **Physical access to an unlocked profile.** Profiles are a data boundary, not a lock.
- **Upstream Chromium vulnerabilities.** We track the pin; we do not audit Blink.
- **The extensions you install.** A recommended extension is still third-party code with page
  access; the catalog states permissions and risk rather than vouching for it.

## Assumptions

- The user's OS, hardware and platform keystore are honest.
- The Chromium sandbox and site isolation are enabled and working (asserted in a test).
- The user's chosen DNS resolver and search engine see what those services normally see; Bedrock
  never inserts its own infrastructure, but it also cannot protect against a resolver the user
  chose.
- Filter lists and catalog metadata come from sources with recorded provenance and are refreshed
  by the user or a signed manifest, not silently by us.

## What Bedrock will never claim

That the user is anonymous, untraceable, invisible, or "100% private". Protection is a set of
mechanisms with states, which is why the UI shows *Privacy protection enabled* and a per-mechanism
configuration view rather than a score. A test enforces the vocabulary.
