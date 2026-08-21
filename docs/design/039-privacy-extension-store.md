# 039 — Bedrock Privacy Extensions (store, analysis, recommendations)

**PrivacyTools.io brief items 1–5, 13, 20, 21.** Status: landed and host-tested
(`src_overrides/bedrock/catalog/`).

The store is a native part of the browser, not an embedded website: entries are local data
(`catalog/bedrock_privacy_catalog.json`), rendered by the browser UI, usable offline.

## The catalog is data, the honesty is code

| Concern | Where it is enforced |
| --- | --- |
| every entry has its own license / source / attribution | `scripts/check_catalog.py` |
| no third-party extension is labelled "official" | catalog gate **and** `BadgeText()` |
| a PrivacyTools.io pick credits PrivacyTools.io | catalog gate |
| verification is not stale | catalog gate (180 days) + `Analysis::verification_stale` |
| a recommendation set has no two tools doing one job | `recommendation_engine_test` |

Badges are derived from **why** an entry is in the catalog: `Recommended by PrivacyTools.io`,
`Recommended by BEDROCK`, `Built into BEDROCK`. There is no code path that produces
"Official BEDROCK Extension" for work Bedrock did not write — item 3 is a missing branch, not a
guideline.

## Analysis before install (items 1, 20)

`Analyze()` returns, for every entry: permission risk (high when the extension can read or change
page content — with the plain-language reason), compatibility (an MV2-only extension is marked
**not installable** rather than failing at install time), maintenance status (an unmaintained
extension with page access is not offered at all), privacy impact, performance impact, and overlap.
Every card exposes **View source / View license / View official repository** before the install
button.

## Overlap and duplicate protection (items 4, 21)

Bedrock already does content blocking, tracker blocking, fingerprint defence, HTTPS enforcement,
script control and passwords. The card leads with *Already protected by BEDROCK*, then names what
the extension adds.

- **substantially duplicate** = every capability it offers is already covered (by Bedrock or by an
  installed extension) → the "Install anyway?" dialog listing the real costs: duplicate
  functionality, more permissions, more attack surface, less compatibility.
- **partial overlap** = it adds at least one capability → shown as *"Adds a specialised layer"*,
  no scary dialog. That distinction is the difference between a curated store and a pile.

## Recommendation engine (item 5)

Three profiles using the Covered / Hardened / Targeted concept (Bedrock's own wording — see
`docs/LICENSING.md` §8). Each one:

- leads with the built-in protections, because the honest answer to "what should I install" is
  usually "nothing else";
- is capped at **three** extensions — the cap is the feature;
- skips anything unusable, stale or fully duplicated by the browser;
- never includes two entries with the same capability;
- carries the disclaimer *"More extensions do not mean more protection."*

Targeted additionally carries a caveat naming the compatibility cost and the fact that a browser
is one part of a threat model. A test scans every profile string for the banned absolutes.

## Refresh without a backend (item 12)

The catalog ships with the browser and can be replaced by a **signed static manifest** — a file in
a Git repository or a release artifact, verified by an injected signature verifier. Refresh is
user-triggered or scheduled; there is no Bedrock server in the path, and a manifest that fails
verification is discarded in favour of the packaged catalog. Updating recommendations, guide
metadata, categories, links or compatibility data therefore does not need a browser release.
