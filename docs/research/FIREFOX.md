# Firefox research

**Roadmap item 49.** What Firefox does, which of it can move into a Chromium-derived browser
without breaking it, and what the licence allows.

**Sourcing, honestly.** This is compiled from Mozilla's public documentation, the published
design notes behind Total Cookie Protection, ETP, RFP, Fission and SmartBlock, and the layout of
mozilla-central as documented publicly — *not* from an audited read of the source tree in this
repository's CI. Every "portable" verdict below is a design judgement, and each one becomes a
design doc with a real source review before any code lands. Nothing in this document is
implemented by this document.

## Licence position

Most of mozilla-central is **MPL-2.0**, the same licence as Bedrock's own code, so file-level
reuse is legally possible — MPL copyleft is per file: a reused file keeps its header, its
licence and an attribution row, and it does not infect Bedrock's other files. That is a much
better position than uBlock Origin or Privacy Badger (GPL-3.0, idea-only).

Three caveats that decide the verdicts below:

1. **Not everything in the tree is MPL-2.0.** Vendored dependencies carry their own licences
   (NSS is MPL-2.0, Rust crates are typically MIT/Apache-2.0, some media and font components
   differ). Per-file check, never per-repository.
2. **Data is not code.** Firefox's tracking protection uses the Disconnect list under its own
   terms, shipped by Mozilla under an agreement that is not ours. Bedrock ships no such list
   (item 12: the user subscribes; list authors' terms apply to them).
3. **Trademarks are not licensed.** No Firefox or Mozilla naming, branding or wordmark.

## Mechanism by mechanism

| Firefox mechanism | What it is | Verdict for Bedrock | Cost / risk |
| --- | --- | --- | --- |
| **Total Cookie Protection (dFPI)** | every site gets its own cookie jar, keyed by top-level site | **Already implemented as the idea** — our `StorageKey = (origin, top-level site, is-cross-site)` covers cookies, storage, cache, DNS, HSTS (item 15). Firefox's exception heuristics (storage-access grants) are worth reading before we write ours. | none — nothing to port |
| **ETP tracker lists** | curated blocklists shipped with the browser | **Idea only, no data.** Licensing (see caveat 2) and item 12's model: user-chosen lists. | none |
| **SmartBlock shims** | tiny stand-in scripts so blocking a tracker does not break the page | **Port the idea, likely reimplement.** Shims are per-site and depend on Firefox's blocking hooks; the shim *catalogue* is the valuable part and is MPL-2.0, so verbatim reuse of individual shim files with attribution is a real option. Fits our pipeline as a stage after a block decision (item 13). | medium; a shim is a compatibility promise that must be maintained |
| **resistFingerprinting (RFP)** | Tor-Uplift normalisation of screen, timezone, fonts, timers, canvas prompts | **Ideas already used** (items 9–10: normalize first, never random per call). Firefox's *letterboxing* for window dimensions is the one concrete piece we do not have and should evaluate. | low for letterboxing, visible UI cost to users |
| **Fission (site isolation)** | per-origin process isolation | **Nothing to port.** Chromium's site isolation predates it and is more mature. What is worth copying is Firefox's public accounting of *what is still shared*, for our threat model. | none |
| **Process architecture (e10s)** | multi-process split | **Not portable** — Chromium's is the foundation (ADR 0001). | — |
| **Permissions** | permission manager with temporary (session) grants and per-site persistence | **Adopt the policy, not the code.** Chromium owns the mechanism (`//components/permissions`, ADR 0003); default-to-temporary and "one grant, one origin, no wildcard" are settings decisions we can make. | low |
| **Storage / "Forget about this site"** | one action removing cookies, storage, cache, history and derived data for a site | **Adopt.** Matches our history-deletion invariant (deleting an entry deletes derived ranking data). Natural extension of `privacy/storage` + `history`. | low; must be honest about what is *not* erased (item 22's rule) |
| **Cookie purging for known trackers** | periodic purge of storage for classified trackers without user interaction | **Adopt the idea**, driven by our own behavioral classifier (item 14) rather than a shipped list. | low |
| **Extension architecture (WebExtensions)** | same API family as Chromium, but Firefox kept **blocking `webRequest`** | **The one real fork in the road.** Chromium MV3 removed blocking `webRequest`; content blockers there are declarative. Bedrock blocks in the browser (item 13), so extensions do not need the API to protect users — but power users expect it. Open question, deliberately not answered here: keeping it means diverging from upstream extension code we otherwise inherit for free. | high — needs its own ADR |
| **Container tabs** | multiple identity containers per profile, separate cookie jars | **Not adopted as-is.** Our workspaces are explicitly *not* privacy boundaries (item 32); containers would blur that. If we want per-container isolation it belongs to profiles (item 21), which already isolate everything. | medium; user-model risk |
| **HTTPS-Only mode** | upgrade with a clear exception flow | **Already implemented** (item 16). Firefox's exception UX is a good reference, nothing to port. | none |
| **DoH / TRR policy** | canary domain, enterprise/parental-control detection, fallback rules | **Adopt selectively.** Our strict mode is fail-closed by design (item 17); Firefox's fallback heuristics are a compatibility layer we may not want, but the *canary* mechanism is worth documenting either way. | low |
| **RLBox** | wrap risky C libraries (fonts, media) into WASM sandboxes | **Strongest genuinely portable security idea here**, and the most expensive. Chromium does not use it; adopting it means owning a build pipeline for sandboxed libraries. Research item, not a plan. | high |
| **Mozilla root store (via NSS)** | Mozilla's own CA program instead of the platform store | **Real option worth an ADR.** A browser-owned root store is a privacy and consistency win; it also means owning root-store updates without our own infrastructure (item 40). MPL-2.0 data. | medium-high |
| **about:memory / performance tooling** | diagnostics | **Skip.** Chromium's equivalents exist. | — |

## What this changes now

Nothing in the code. Three follow-ups are recorded for later roadmap items, in the order they
would pay off:

1. **Letterboxing** as an anti-fingerprinting surface (extends item 9's surface list).
2. **"Forget about this site"** as one action across `privacy/storage`, `history`, `bookmarks`
   and the event log.
3. **Blocking `webRequest`: keep or drop** — needs its own ADR, because it decides how far the
   extension system diverges from upstream Chromium.

Two are deliberately parked: RLBox and a Bedrock root store. Both are real, both are large, and
neither should start before a Chromium build runs in CI.

## What we will not take

- Firefox's tracker list data (licensing, and item 12's model).
- Gecko-side code with no Chromium equivalent — porting an engine subsystem into a different
  engine is a rewrite wearing someone else's licence header.
- Any Mozilla branding, naming or trademark.
- Anything that would require Bedrock to run a service (caveat: item 39/40 — Mozilla's remote
  settings pipeline is exactly such a service, and we do not want one).
