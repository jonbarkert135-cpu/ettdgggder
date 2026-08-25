# What Bedrock is, and what it refuses to be

**Roadmap items 92, 96, 97.** [`BRAND.md`](BRAND.md) says what Bedrock looks like.
[`PROVENANCE.md`](PROVENANCE.md) says where its files come from. This says what it *is*, and
draws the two lines that are easy to cross by accident: another vendor's trademark standing in
for ours, and another vendor's interface standing in for ours.
`scripts/check_trademarks.py` enforces the checkable half.

## One product

Bedrock is a serious, independent, privacy-first browser built on Chromium: Chromium's site
compatibility and sandbox, protection in the spirit of Brave's Shields, anti-fingerprinting
reasoning learned from the Tor Browser's design document, privacy engineering in the tradition of
Firefox's isolation work, blocking of uBlock Origin's class, and Privacy Badger's insight that a
tracker can be recognised by its behaviour rather than by a list.

That list is a set of influences, not a parts catalogue. **Bedrock is one product, not five
browsers wearing one skin.** The test we apply to any borrowed idea is whether a user could tell
where it came from: if a feature only makes sense to someone who already used the browser it was
taken from, it has not been designed yet. One settings model, one vocabulary for protection
levels, one Privacy Center that explains every mechanism in the same voice, one visual language
from `branding/design-tokens.json`.

Two things Bedrock is deliberately not:

- **Not a skin over Chromium.** The privacy mechanisms are the product; the chrome around them is
  how they become usable. A theme with no enforcement behind it is what item 90 calls a decorative
  feature pretending to be functional.
- **Not anonymity software.** It is not the Tor Browser and does not compete with it. The onboarding
  says so in the user's own language ("Privacy protection is not invisibility"), and
  `browsing_mode.cc` carries the disclaimer in the product.

## Bedrock does not copy other browsers' interfaces (item 97)

Not Firefox's, not Brave's, not Safari's, not Chrome's, not Edge's. Studying how a browser solves a
problem is normal engineering; reproducing its layout, its iconography, its control shapes or its
CSS vocabulary is not. In practice:

- No other browser's design tokens, class names, id prefixes or stylesheet structure appear in our
  UI — checked mechanically (`--moz-`, `--brave-`, `.chrome-` and relatives fail the build).
- No other vendor's icon, logo or image file exists anywhere in the tree, under any filename —
  also checked.
- Where a comment says "the part Chrome gets right", it names a *behaviour we agreed with* and is
  followed by our own implementation. That is the only form the influence may take, and comments
  are exempt from the vocabulary check for exactly this reason: the reasoning stays visible.

## Other people's names (item 92)

Bedrock never uses another vendor's name as its own name, never ships their logo, and never implies
they endorsed, approved or partnered on this browser. It is not "Bedrock Chrome", there is no
"official" anything, and nothing is "powered by" a company that has never heard of this project.

Naming another browser *as that browser* is different, legitimate, and required by the product:
the import step lists Chrome, Firefox and Edge because that is what the user is importing from
(item 98); the search step names Google and DuckDuckGo because that is where the query is going
(item 93); the DoH presets name their operators because a user choosing a resolver has a right to
know whose it is; and the Tor disclaimer must say "Tor Browser" to deny the association. The gate
distinguishes the two: a vendor name near words like *official*, *certified*, *powered by* or
*in partnership with* fails; a vendor name describing that vendor's own product does not.

## Stated influences

Each research note takes a position, and the gate requires one — a project cannot be studied into
the product without a public statement of what was taken and what was refused.

| Influence | What Bedrock took | What it did not take |
| --- | --- | --- |
| [`research/BRAVE.md`](research/BRAVE.md) | The Shields *model*: per-site protection with a visible ledger; farbling as a published approach | No brave-core file, no Shields UI, no Brave name or mark. Mode `reimplement` in the inventory |
| [`research/FIREFOX.md`](research/FIREFOX.md) | The privacy engineering tradition: total cookie protection, container-style isolation, RFP's reasoning | No Gecko code (it does not fit a Chromium base anyway), no Firefox UI, no Photon visual language |
| [`research/TOR_BROWSER.md`](research/TOR_BROWSER.md) | The design document's threat reasoning: uniformity over randomness, letterboxing, security levels | The name, the claim, the network guarantees, and any suggestion of affiliation with the Tor Project |
| [`research/UBLOCK_ORIGIN.md`](research/UBLOCK_ORIGIN.md) | The documented ABP/uBO filter *syntax* and the pipeline shape | No GPL-3.0 code, no list files, no reading of its source for implementation guidance |
| [`research/PRIVACY_BADGER.md`](research/PRIVACY_BADGER.md) | The published heuristic: a third party seen tracking on three sites is a tracker; GPC and link cleaning | No GPL-3.0 code, no yellow list, no EFF branding |
| [`research/ORIGIN_TOOLS.md`](research/ORIGIN_TOOLS.md) | Nothing — the project named in the brief does not exist | No invented dependency to fill the gap (item 90) |

Adding a note under `docs/research/` without adding a row here fails the build.
