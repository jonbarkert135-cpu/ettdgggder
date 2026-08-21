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


## The adversaries (roadmap item 44)

Fourteen adversaries, each with what Bedrock does and where it stops. "Where it stops" is the
column that makes the rest of the table worth reading.

| Adversary | What Bedrock does | Where it stops |
| --- | --- | --- |
| **Malicious website** | Site isolation and the Chromium sandbox stay on (asserted by `security_baseline_test`, 13 required features / 14 forbidden switches); no privacy feature may weaken them; permissions are per-origin and revocable | A site the user grants access to gets what they granted |
| **Malicious ads** | Ads are blocked before the request leaves (`BlockingPipeline`), third-party frames are storage-partitioned, active mixed content is always blocked | An ad served first-party from the site's own origin is indistinguishable from content |
| **Trackers** | Filter lists plus a local behavioural heuristic (3 distinct first parties ⇒ tracker), tracking parameters stripped, referrer reduced to origin | First-party analytics the site runs itself |
| **Cross-site tracking** | One `StorageKey` (origin, top-level site, is-cross-site) across all 13 storage backends including HTTP cache, DNS cache and HSTS; third-party cookies blocked by default; Storage Access API scoped | Correlation by login: signing into the same account on two sites links them regardless |
| **Fingerprinting** | Normalisation first, perturbation only where normalisation is impossible, deterministic per (session secret, eTLD+1, surface) so averaging does not defeat it; 21 documented surfaces; 4 levels | A sufficiently determined script can still estimate a bucket. Bedrock raises cost; it does not deliver a single indistinguishable identity, and never claims to |
| **Compromised extension** | Permissions disclosed from the manifest before install; an update that widens permissions is staged for review, never auto-applied; extensions off in private windows by default; catalog states risk rather than vouching | An extension the user grants page access to can read those pages. That is what the permission means |
| **Malicious downloads** | Local risk assessment: executables, deceptive double extensions, U+202E overrides, promised-type mismatch; explicit confirmation before keeping; nothing is auto-opened | No cloud reputation service (we have no backend), so a novel malicious document looks ordinary |
| **Local attacker** | Profiles are separate data roots; the password store is encrypted by the platform keystore, locked by default, with idle auto-lock and optional master password; temporary profiles never touch disk | An unlocked machine, or malware running as the user, defeats all of it |
| **Network observer** | HTTPS upgrade by default and HTTPS-only available; no plaintext fallback in strict mode; encrypted DNS optional; Tor transport mode with per-site circuit isolation | Traffic volume and timing remain visible; an observer at both ends of a Tor circuit is out of scope for any browser |
| **DNS observer** | System resolver by default (the honest default: it is what the user already chose), DoH/DoT configurable, strict mode fails closed rather than leaking; six known leak paths documented in `docs/design/017` | A resolver the user configures sees their queries. Bedrock never inserts its own — that is the point, and also the limit |
| **Malicious Wi-Fi** | HTTPS-first with no global certificate bypass; revoked, pin-mismatched and weak-signature certificates can never be excepted; captive-portal detection never disables warnings | The user can still accept a warning for a self-signed certificate they believe in |
| **Compromised third-party dependency** | Provenance-first policy: pinned versions, notice files, an SBOM generated from the inventory, dependency hashes where verified; no unpinned dependency may ship; catalog entries carry their own license and source | A dependency compromised upstream at a pinned-and-hashed version would still be shipped until it is discovered — pinning is integrity, not omniscience |
| **Browser exploit** | Upstream Chromium security fixes arrive by moving the pin; the update path refuses unsigned, unknown-key, mismatched-hash, downgraded and non-HTTPS releases; the security baseline is a test | A zero-day in Blink is a zero-day here. We do not audit the renderer |
| **Renderer exploit** | Sandbox and site isolation mandatory; no privacy feature may relax them; a compromised renderer still faces per-site process boundaries and partitioned storage | A sandbox escape in upstream Chromium defeats the browser. Bedrock's contribution is not making it easier |

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
