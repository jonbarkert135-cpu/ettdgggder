# ADR 0010 — Tor is a browsing mode with honest limits, not a rebranded Tor Browser

**Status:** accepted (2026-08-21) · roadmap items 19, 22, 51 · owner's list: ADR-006
**Design notes:** `docs/design/019-browsing-modes.md`, `docs/design/022-new-identity.md`
**Research:** `docs/research/TOR_BROWSER.md`

## Context

Routing traffic through Tor is easy. Producing the protection people believe Tor Browser gives
them is not: Tor Browser's value is the *uniformity of its users*, achieved through years of
patches, a fixed window size, disabled features and a shared fingerprint. A Chromium-based
browser that adds a SOCKS proxy and calls it Tor mode gives its users a distinctive fingerprint
inside the anonymity set, which is worse than useless — it is a false sense of safety.

The Tor Project's trademark position is also explicit, and item 51 repeats it: do not copy Tor
branding.

## Decision

Bedrock ships **Tor mode** as a browsing mode, with three commitments:

1. **Naming and expectation management.** It is called "Tor mode", never "Tor Browser", never
   with Tor's onion mark. The mode's entry screen states in one screen what it does and does not
   protect, including the sentence that matters: *your fingerprint here is a Bedrock fingerprint,
   not a Tor Browser fingerprint.*
2. **Level 3 fingerprinting is forced on in this mode**, together with the strictest storage,
   referrer and WebRTC policies. Uniformity is not achievable, but distinguishing information is
   minimised rather than left at Balanced.
3. **New Identity is a real operation**, not a cache clear: circuit change, session seed
   regeneration (so every perturbation value changes), storage teardown and window reset,
   verified as one atomic action.

## Alternatives considered, and one deliberately left open

Whether Bedrock **bundles** a tor daemon or requires a system one. Bundling means shipping,
signing, updating and taking responsibility for a security-critical C binary and its consensus
handling; not bundling means a worse first-run experience. The decision waits for the Chromium
build, and until then Tor mode is `Status::kDesigned` — the tests for proxy routing and DNS
behaviour are in `tests/matrix.json` marked `needs-network`, which is the honest state.

## Consequences

* `.onion` support, stream isolation per site and the bridge configuration surface are tracked as
  follow-ups, not implied by "Tor mode exists".
* No Tor Project code is copied; interaction is over the control and SOCKS protocols, which are
  specified in public.
