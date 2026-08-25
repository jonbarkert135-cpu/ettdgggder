# Brave research (brave-core)

**Roadmap item 50.** What brave-core does, what Bedrock can legally take, and what it must
refuse. Brave is the closest relative of this project: same base engine, same licence family,
overlapping goals — which makes the line between *reuse* and *clone* worth drawing precisely.

**Where the code is.** `brave/brave-core` holds the browser source and the Chromium patches;
`brave/brave-browser` holds issues, releases and build tooling. Research reads brave-core.
Pinned in the inventory at **v1.96.5, MPL-2.0, reuse mode `reimplement`** — no brave-core file is in this tree ([`PROVENANCE.md`](../PROVENANCE.md))
([`docs/THIRD_PARTY.md`](../THIRD_PARTY.md)).

**Sourcing, honestly.** Compiled from Brave's public documentation, published design notes
(farbling, Shields, query filtering, ephemeral storage) and the documented structure of
brave-core — not from an audited read of the tree in this repository's CI. Every "port"
verdict below becomes a design doc with a file-level source and licence review before code
lands. Nothing here is implemented by this document.

## Licence position

brave-core is **MPL-2.0**, the same licence as Bedrock's own code, and MPL copyleft is
**per file**. So, uniquely among our sources, *literal file reuse is available*: a ported file
keeps its MPL header, gains a `Modified by the Bedrock authors` line, and is listed in
`THIRD_PARTY_NOTICES/brave-core.txt` with its exact upstream path and commit. That is already
the recorded reuse mode.

What the licence does **not** cover, and we therefore refuse regardless of technical merit:

- **Brave's trademarks and product names** — no Brave naming anywhere.
- **Brave's services**: Rewards/BAT, Brave Search, Brave News, Wallet, Talk, VPN, sync. All
  require infrastructure; Bedrock runs none (invariant 1). This is the biggest single
  difference between the two browsers and it is a product decision, not a licensing one.
- **Brave's list-distribution CDN and component updater.** Brave ships filter lists through its
  own servers. Bedrock fetches lists from the list authors, on the user's machine (item 12).

## Mechanism by mechanism

| Brave mechanism | What it is | Verdict for Bedrock | Notes / cost |
| --- | --- | --- | --- |
| **Shields** (per-site model) | one panel with per-site ad/tracker, cookie, fingerprint and HTTPS controls, stored as content settings | **Already implemented as the idea** — our Protection Controller (item 11) resolves site → domain → global → built-in, and `PrivacyPolicy` (item 25) keeps the layers consistent. Brave's *content-settings storage* pattern is worth copying at implementation time. | low |
| **Farbling** (fingerprint randomisation) | deterministic per-session, per-eTLD+1 perturbation of canvas, WebGL, audio, plugins, hardware concurrency, UA | **Idea already adopted, philosophy deliberately different.** We normalize first and perturb only canvas/WebGL/audio (items 9–10). Brave's session-key + domain-key derivation is the closest published relative of our deterministic derivation; their *key rotation per session* is worth adopting explicitly. | low |
| **adblock-rust** | the Rust matching engine behind Brave's blocker | **Already decided: swappable backend, not the default** (ADR 0002). MPL-2.0, so vendorable as a pinned crate — and it would be the first Rust in the tree (ADR 0004). | medium: Rust toolchain in the Chromium build |
| **CNAME uncloaking** | resolve a subdomain's CNAME before matching, so a first-party-looking tracker is still caught | **Adopt.** This is the single most valuable blocking mechanism we do not have. It costs a DNS lookup on the blocking path and must respect our DNS settings (item 17) — an uncloaking lookup that bypasses the user's resolver choice would be a privacy bug of its own. | medium |
| **Query filter** (URL parameter stripping) | strips known tracking parameters (`fbclid`, `gclid`, …) from navigations | **Adopt**, as a stage of the one pipeline (item 13), never as a second blocker. Needs a per-site off switch: stripping breaks some logins and campaign attribution the *user* wanted. | low |
| **Debouncing** | skips known redirect-tracker hops and navigates straight to the destination | **Adopt the idea**, rule data reimplemented. Same pipeline stage as query filtering. | low-medium |
| **Referrer handling** | cross-site referrers reduced to the origin, or dropped entirely | **Adopt as policy.** Chromium's default is already `strict-origin-when-cross-origin`; Brave goes further. Belongs in `privacy/network` with the other network policy. | low |
| **Client Hints** | most UA-CH disabled or frozen | **Adopt** — our client-hints surface is already documented (`docs/privacy/fingerprinting/client-hints.md`); Brave's choice of *which* hints to freeze is a useful cross-check. | low |
| **Language / Accept-Language reduction** | `navigator.languages` and `Accept-Language` reduced (often to `en-US`) at higher levels | **Adopt with the cost stated.** This is a real entropy cut and a real usability loss for non-English users — exactly the kind of thing our levels must price openly (item 45, "every level above Standard names its cost"). | low, high user impact |
| **Ephemeral storage** | third-party storage that lives only as long as the tab/site session | **Idea overlaps our StorageKey partitioning** (item 15); the *lifetime* half (auto-discard at session end) is not something we do. Worth evaluating as a storage-lifetime option rather than a new isolation model — two isolation models would violate invariant 4/5. | medium |
| **Scriptlet / resource replacement** (`$redirect`, SugarCoat-style stand-ins) | serve a neutered stand-in instead of blocking outright, so the page still works | **Already partly present** (our engine supports `$redirect` resources); the *catalogue* of stand-ins is the work. MPL-2.0 pieces in brave-core are reusable file-by-file. | medium; a stand-in is a maintenance promise |
| **de-AMP / Google sign-in blocking** | bypass AMP pages; block the Google auth iframe by default | **Adopt de-AMP**, small and self-contained. Sign-in blocking overlaps our third-party storage policy already. | low |
| **Localhost / private-network access blocking** | pages may not probe `127.0.0.1` or LAN addresses | **Adopt** — matches our WebRTC and network-privacy posture (item 18). | low |
| **Brave Rewards / BAT, Search, News, Wallet, Talk, VPN, Sync** | Brave's product and revenue surface | **Refuse, all of it.** Each needs a server; Bedrock operates none (invariant 1). | — |
| **Brave's update + component pipeline** | Brave-run update and list delivery | **Refuse the infrastructure**, keep the shape: our updater is provider-agnostic and signature-verified (item 40). | — |

## What this changes now

Nothing in the code. Recorded follow-ups, in the order they pay off:

1. **CNAME uncloaking** in the blocking pipeline, using the user's configured resolver.
2. **Query stripping + debouncing** as one navigation-cleaning stage with a per-site switch.
3. **Referrer and Client-Hints policy** written into `privacy/network` and the level ladder.
4. **Language reduction** as a priced level control, not a silent default.

Parked deliberately: ephemeral storage lifetimes (needs a decision against our single isolation
model), adblock-rust adoption (needs the Rust toolchain, ADR 0002/0004).

## What we will not take

- Anything requiring a Brave-run service, or any Brave branding.
- Brave's list distribution and component updater.
- A file-level port without a per-file licence header check, a notice-file row and a recorded
  upstream commit — MPL is per file, and "brave-core is MPL" is not evidence about *this* file.
