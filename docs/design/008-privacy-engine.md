# 008 — Privacy Engine

**Roadmap item 8.** Status: architecture + feature registry landed
(`src_overrides/bedrock/privacy/privacy_engine.h`).

## The rule that shapes everything

Privacy is **one subsystem with one posture**, not a page of unrelated checkboxes.
The user picks a **level** (Standard / Strict) for the profile and, if needed, overrides a
single site. Individual feature toggles exist, but touching one moves the profile to `Custom`
and the UI says so.

**Every feature must be explainable.** `FeatureInfo` requires an `explanation_string_id`;
the settings page and the shields panel are generated from the registry, so a control with no
plain-language explanation literally cannot be rendered — and therefore cannot ship.

## Five modules

| Module | Runs in | Owns |
|---|---|---|
| **Content blocker** | browser + renderer | tracker protection, ad blocking, cosmetic filtering, cross-site tracking |
| **Network** | network service | referrer control, URL parameter stripping, HTTPS-only, secure DNS, third-party request control |
| **Storage** | browser + network service | cookie isolation, storage partitioning, ephemeral third-party storage |
| **Fingerprint** | renderer (Blink bindings) | canvas, WebGL, fonts, client hints, language/timezone/screen normalization, hardware info, battery, WebRTC, timer coarsening |
| **Permissions** | browser | media device enumeration, sensors/gamepad, clipboard, geolocation, notifications, autoplay, permission isolation per site |

A feature belongs to exactly one module. That is what makes a regression traceable: a broken
site is either a blocker rule, a network rewrite, a storage partition or an API shim — never
"privacy, somewhere".

## Content blocker

Engine: **adblock-rust** (Brave, MPL-2.0, v0.13.3) — full uBO/ABP network + cosmetic syntax,
license-compatible, already battle-tested at Brave's scale. See `docs/THIRD_PARTY.md` for why
uBlock Origin's own code (GPL-3.0) cannot be linked in, and `docs/design/009-extension-catalog.md`
for how users still get uBO if they want it.

Lists are **data fetched at runtime** from the list authors' own URLs (EasyList, EasyPrivacy,
uBO filters), on a schedule, directly from the user's machine — no Bedrock CDN, no Bedrock
list proxy. Default: EasyList + EasyPrivacy + uBO badware/privacy lists.

## Fingerprinting: the Tor lesson, applied honestly

Two schools: **uniformity** (Tor Browser: everyone looks identical) and **randomisation**
(Brave: everyone looks different every time). Bedrock's default is **Standard = randomisation
per (site, session)** because uniformity only works if the whole population runs identical
builds, which Bedrock's population will not. **Strict adds uniformity** where it does not
break the page: quantised window/screen metrics (letterboxing), `en-US` language, UTC timezone,
a fixed font list, reduced `hardwareConcurrency`/`deviceMemory`, coarsened timers.

Each shim returns a *plausible* value, never an obviously fake one — a value that screams
"anti-fingerprinting browser" is itself a fingerprint. The design source is the public Tor
Browser design document; no Gecko code is used (see `THIRD_PARTY_NOTICES/tor-browser.txt`).

## Tracker learning (Privacy Badger's idea, our code)

A local heuristic complements the lists: a third-party origin observed setting identifying
state (cookie, localStorage, high-entropy fingerprint call) on **≥ 3 distinct first-party
sites** gets blocked or cookie-blocked. The learned table is per profile, stored locally, never
uploaded, and clearable. Independently implemented from EFF's published description — no
Privacy Badger code, lists or yellow list (GPL-3.0, see notices).

## Storage & network defaults

- **Total cookie protection**: third-party cookies and storage are partitioned per top-level
  site. Third-party storage is ephemeral by default in Strict.
- **Referrer**: cross-origin referrers trimmed to the origin; Strict sends none.
- **Query parameter stripping**: known tracking params (`fbclid`, `gclid`, `utm_*`, …) removed
  on navigation, from a list shipped as data with a per-site opt-out for sites that break.
- **HTTPS-only** on by default with an interstitial, not a silent downgrade.
- **Secure DNS**: off by default (the system resolver is the user's choice); when enabled, the
  user picks the resolver from a list and can type their own. Bedrock runs no resolver.
- **WebRTC**: default policy prevents local-IP leakage without disabling calls.

## User-facing surface

- **Shields panel** (per site): level, what was blocked, one-click "off for this site" that
  reloads. Counts come from the local action log.
- **Settings → Privacy**: level selector, then the registry grouped by module, each row with
  its explanation and a `breaks_sites` warning where applicable.
- **`bedrock://privacy-log`**: recent `Action` records — feature, site, detail. Local only,
  memory-capped, cleared on exit unless the user pins it. This is the "explain it to me" tool:
  if a site broke, the log names the exact feature that did it.

## What we will not do

No privacy score, no "protected N trackers today" gamification, no cloud-assisted anything,
no VPN or proxy of ours, no allow-list deals with sites. Every mechanism is local and off-switchable.
