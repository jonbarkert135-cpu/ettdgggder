# Tor Browser research

**Roadmap item 51.** What Tor Browser does, what a Chromium-derived browser can honestly take
from it, and the one claim we must never make.

**Sourcing, honestly.** Compiled from the Tor Project's public design document, the Tor Browser
release notes and the published rationale behind its security levels and anti-fingerprinting
work — not from an audited read of tor-browser in this repository's CI. Tor Browser is
Firefox-derived, so most of its patches are Gecko-specific and unavailable to us as code
regardless of licence.

## The honesty rule comes first

Tor Browser's anti-fingerprinting works because **its users share one browser, one window size
policy, one locale, one font set and one timer resolution**. The protection is the *crowd*, not
the code. Bedrock is Chromium-derived: our users are in a different, much smaller pool, and
copying Tor's individual mitigations does not copy its anonymity set.

Therefore, and this is already invariant 9 and a string-level test in
`session/browsing_mode.h`:

- Tor Mode is a **transport**, not an anonymity guarantee and not a rung on the security ladder.
- No user-visible string says *anonymous*, *untraceable*, *invisible* or *100%*.
- We do not claim Tor Browser's fingerprinting properties. We say what a control does.

## Licence and trademark position

- Tor Browser code is **MPL-2.0** (Firefox-derived) — legally reusable per file, practically
  unusable: it patches Gecko internals that have no Chromium counterpart.
- The **tor daemon** is BSD-3-Clause and is a separate program. Bedrock does not bundle it
  today; a Tor window uses a SOCKS proxy the user provides. Bundling would be legal, but it
  would make us responsible for shipping and updating an anonymity network client — a
  commitment we have not made.
- **The name is restricted.** The Tor Project limits use of the trademark for modified builds.
  Bedrock ships no product named "Tor"; the setting describes a transport.

## Mechanism by mechanism

| Tor Browser mechanism | What it is | Verdict for Bedrock | Notes / cost |
| --- | --- | --- | --- |
| **Uniformity over randomisation** | make every user look identical, rather than each user look different | **Adopted as our first anti-fingerprinting rule** (item 9: normalize first, never random per call). The single most valuable idea in this document. | — |
| **Letterboxing** | window content sized to quantised steps so window dimensions leak little | **Adopt.** Chromium-implementable (it is a UI-level constraint, not a Gecko internal) and it closes a surface we currently only document. Visible to users, so it belongs to the higher levels with its cost stated. | medium; UX cost |
| **Timer coarsening + jitter** | reduce `performance.now()` resolution to blunt timing side channels | **Already in our surface list**; Tor's chosen values are a reference point, not a target — Chromium's clamping differs. | low |
| **Locale / timezone / language normalisation** | report one locale, UTC, one language set | **Already adopted** (documented surfaces). Cost stated at the level, as with Brave's language reduction. | low, high user impact |
| **Font enumeration limits** | restrict font lists to a fixed bundle | **Adopt the idea, not the values.** Chromium's font stack differs per platform; a fixed allowlist is a compatibility decision that needs its own design doc. | medium |
| **Security levels: Standard / Safer / Safest** | one slider that turns off JIT, WASM, media autoplay, JS on non-HTTPS, SVG/fonts at the top | **Partly adopted, deliberately different.** Our ladder (Standard / Balanced / Strict / Maximum, item 45) is about *privacy* controls; Tor's is about *attack surface*. Worth taking: the explicit "this will break sites" wording at the top rung, which we already require. Worth evaluating: exposing a JIT-disable control, which Chromium supports (`--js-flags=--jitless`, per-site JIT policy) and which is the strongest single hardening switch a user can flip. | medium; JIT-off is a real perf cost |
| **New Identity / New Circuit for this Site** | full session reset vs. per-site circuit change | **Both already modelled** (items 20/22 for identity, per-site circuit isolation in `session/browsing_mode.h`). Tor's split between the two is exactly the distinction our New Identity plan makes: what is reset and what is not. | — |
| **Stream isolation via SOCKS credentials** | different SOCKS username/password ⇒ different circuit, so two sites cannot share an exit | **Already implemented in the model:** `CircuitId{socks_username = top-level site, socks_password = identity epoch}`. This research confirms the shape rather than adding to it. | — |
| **Circuit display in the UI** | the site info panel shows the actual relays for the current site | **Adopt when a Tor window has a real transport.** It is the honest counterpart to not promising anonymity: show what the transport is doing instead of asserting a property. | low-medium |
| **HTTPS-Only by default** | HTTPS-Only mode is on in Tor Browser | **Already implemented** (item 16); in Tor mode it should be forced, not merely defaulted. Small change, worth recording. | low |
| **Site isolation** | Tor Browser inherits Firefox's Fission | **Nothing to port** — Chromium's site isolation is the foundation and is more mature. | — |
| **.onion support** | first-class onion addresses, onion-location | **Out of scope today.** It requires the bundled daemon decision above. Recorded, not planned. | high |
| **Tor branding, "anonymous" wording** | — | **Refused**, see the honesty rule. | — |

## What this changes now

Nothing in the code. Recorded follow-ups:

1. **Letterboxing** (also arrived at from the Firefox research — same mechanism, same verdict).
2. **Force HTTPS-Only inside Tor windows**, rather than inheriting the profile default.
3. **Circuit display** in the site panel once a transport is wired.
4. **Evaluate a JIT-disable control** as an attack-surface rung, separate from the privacy ladder.

Parked: bundling the tor daemon, `.onion` support, font allowlisting.

## What we will not take

- The claim. Tor Browser's anonymity properties belong to Tor Browser's users and its network,
  not to a Chromium browser that speaks SOCKS.
- Tor branding or any implication of endorsement by the Tor Project.
- Gecko-specific patches — porting them into Blink is a rewrite wearing someone else's header.
