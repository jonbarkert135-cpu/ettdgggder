# Bedrock Browser — Third-Party Inventory

Machine-checked by `scripts/check_provenance.py`. Do not edit the table format.
Versions verified against upstream on **2026-08-21**.

<!-- BEGIN INVENTORY -->
| Project | Repository | Pinned version | License | Reuse mode | Notice |
|---|---|---|---|---|---|
| Chromium | https://chromium.googlesource.com/chromium/src | 151.0.7922.173 | BSD-3-Clause | patched-base | chromium.txt |
| brave-core | https://github.com/brave/brave-core | v1.96.5 | MPL-2.0 | port | brave-core.txt |
| adblock-rust | https://github.com/brave/adblock-rust | v0.13.3 | MPL-2.0 | vendored | adblock-rust.txt |
| ungoogled-chromium | https://github.com/ungoogled-software/ungoogled-chromium | 151.0.7922.169-1 | BSD-3-Clause | port | ungoogled-chromium.txt |
| uBlock Origin | https://github.com/gorhill/uBlock | 1.73.0 | GPL-3.0-or-later | separate-artifact | ublock-origin.txt |
| Privacy Badger | https://github.com/EFForg/privacybadger | release-2026.8.7 | GPL-3.0-or-later | separate-artifact | privacy-badger.txt |
| Tor Browser (tor-browser) | https://gitlab.torproject.org/tpo/applications/tor-browser | not-pinned-yet | MPL-2.0 | reimplement | tor-browser.txt |
| Firefox (mozilla-central) | https://github.com/mozilla-firefox/firefox | not-pinned-yet | MPL-2.0 | reimplement | firefox.txt |
| PrivacyTools.io | https://www.privacytools.io | catalog-snapshot-2026-08-21 | VERNAM License | vendored | privacytools-io.txt |
<!-- END INVENTORY -->

`not-pinned-yet` is only legal for reuse mode `reimplement` (no code enters the tree, so there
is nothing to pin); the checker enforces this.

---

## Chromium — `patched-base`

- **Copyright:** © The Chromium Authors. Chromium itself aggregates hundreds of licenses
  (see its own `LICENSE` and `about:credits`).
- **What we use:** the whole source tree as the browser base, fetched by
  `build/sync.py` at the pinned tag; we never fork it into this repo. Our changes live in
  `patches/` and `src_overrides/`.
- **Attribution:** BSD-3 notice retained; Chromium's generated credits page ships in the binary.
- **Redistribution:** binary and source redistribution allowed with notice.
- **Source disclosure:** none required by BSD-3; we publish patches anyway.
- **Trademark:** must strip `Chrome`/`Google` branding, Google API keys, and default Google
  service endpoints. See `docs/adr/0001-chromium-overlay.md`.
- **Compatibility:** BSD-3 + MPL-2.0 combination is fine; MPL files remain individually MPL.

## brave-core — `port`

- **Copyright:** © The Brave Authors.
- **What we use:** targeted files/ideas for privacy mechanisms (fingerprint farbling design,
  shields settings model, ad-block service wiring). Each ported file keeps its MPL-2.0 header
  plus a `Modified by the Bedrock authors` line and is listed in `THIRD_PARTY_NOTICES/brave-core.txt`
  with its exact upstream path and commit.
- **Redistribution / disclosure:** MPL-2.0 — ported files and our modifications to them stay open.
- **Trademark:** no Brave naming, no Brave Rewards/BAT, no Brave update or Brave Search endpoints.
- **Compatibility:** MPL-2.0 file-level copyleft; compatible with our MPL-2.0 overlay and with
  Chromium's BSD base.

## adblock-rust — `vendored`

- **Copyright:** © The Brave Authors.
- **Status at this commit: not in the tree.** No Rust code, no vendored crate. The row above
  records the mode we would use *if* the backend is adopted (ADR 0002).
- **What we would use:** the crate as-is, as an alternative matcher behind
  `bedrock::blocking::FilterEngine`. Consumed as a pinned crate, not copied file-by-file.
- **Trademark:** none used.
- **Compatibility:** MPL-2.0; Rust-in-Chromium is supported by the upstream build.

## ungoogled-chromium — `port`

- **Copyright:** © The ungoogled-chromium contributors.
- **What we use:** the *approach* and, where directly applicable, individual de-Google patches
  (domain substitution, binary pruning, disabling of Google-bound services). Any patch adopted
  verbatim is stored under `patches/upstream/ungoogled/` with the upstream path recorded.
- **Compatibility:** BSD-3, no obligations beyond notice.

## uBlock Origin — `separate-artifact` ⚠ GPL-3.0

- **Copyright:** © Raymond Hill (gorhill) and contributors.
- **Decision:** **not vendored, not linked, not ported.** GPL-3.0 would relicense the browser
  binary. See `docs/LICENSING.md` §3.
- **What is allowed instead:** (a) supporting uBO as a user-installed WebExtension (aggregation);
  (b) reimplementing the documented ABP/uBO filter syntax — this is what Bedrock does, see
  `src_overrides/bedrock/blocking/filter_engine.cc`; (c) using `adblock-rust`, which already
  implements that syntax under MPL-2.0.
- **Filter lists** are separate works with their own licenses (EasyList: CC BY-SA 3.0 / GPL-3.0
  dual) and are fetched at runtime, never compiled in.
- **Trademark:** "uBlock Origin" used descriptively only.

## Privacy Badger — `separate-artifact` ⚠ GPL-3.0

- **Copyright:** © 2015 Electronic Frontier Foundation and contributors.
- **Decision:** same as uBO — no code, no lists, no yellow-list copying.
- **What is allowed instead:** reimplementing the published heuristic (a third-party origin
  observed setting identifying state on ≥3 first-party sites gets blocked/cookie-blocked) from
  EFF's public write-ups. Implementation must be independently written and header-marked as
  "inspired by, not derived from".
- **Trademark:** EFF names/logos not used.

## Tor Browser — `reimplement`

- **Copyright:** © The Tor Project and Mozilla contributors.
- **Decision:** Tor Browser is Firefox-derived; its patches are Gecko-specific and cannot be
  applied to Chromium. We take **anti-fingerprinting design only** (letterboxing/resolution
  quantisation, timer coarsening, locale/timezone/language normalisation, font enumeration
  limits, uniformity-over-randomisation philosophy) from the public design document.
- **Trademark:** the Tor Project restricts the name for modified builds — Bedrock ships **no**
  feature named "Tor". No Tor network integration is bundled.

## Firefox (mozilla-central) — `reimplement`

- **Copyright:** © Mozilla Foundation and contributors.
- **Decision:** architecture-level inspiration only (Total Cookie Protection / dynamic first-party
  isolation, Enhanced Tracking Protection tiering, container-style profile separation). Gecko code
  is not portable to Chromium; if any MPL-2.0 file were ever ported it would move to reuse mode
  `port` with a pinned revision.

## PrivacyTools.io — `vendored`

- **Copyright:** © 2026 PrivacyTools.io.
- **What we use:** their *recommendations and reference metadata* — which extensions they list,
  the threat-level concept, and links to their guides. Curated into
  `src_overrides/bedrock/catalog/bedrock_privacy_catalog.json`. No article text is copied into
  the browser; external material is shown as a link card (`docs/design/040`).
- **Attribution:** required. The VERNAM License is permissive (a WTFPL derivative) with one
  binding condition: a clear, visible, working credit link back to https://www.privacytools.io
  wherever the work is used, with no implication of endorsement. Bedrock shows
  `Privacy recommendations and selected reference materials: PrivacyTools.io` with that link in
  the extension store and the Knowledge Center, and `scripts/check_catalog.py` fails the build if
  the attribution is missing.
- **Redistribution:** allowed under the same condition.
- **Source disclosure:** none required.
- **Trademark:** "PrivacyTools.io" and "VERNAM" are their names, not ours. Used descriptively
  only; Bedrock never implies they endorse it, and never brands its own UI with their marks.
- **Compatibility:** compatible with MPL-2.0; their material stays under their license and is
  labelled as theirs.
- **Third-party material:** their license covers *their* work. Every tool, guide or trademark
  they link to (uBlock Origin, ClearURLs, Cookie AutoDelete, Decentraleyes, Tor Project, Mozilla,
  EFF …) keeps its own license, and each catalog entry carries its own `license`,
  `official_source` and `attribution` fields — enforced by `scripts/check_catalog.py`.
- **Verified:** 2026-08-21 against https://privacytools.io/license and
  https://www.privacytools.io/privacy-browser-addons.
