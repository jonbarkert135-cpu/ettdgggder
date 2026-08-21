# Filter lists — licences, one per list

**Roadmap item 52.** Filter lists are *data*, they are **not** covered by uBlock Origin's
licence, and they are not covered by each other's. "The filter set is GPL" and "the filter set
is CC BY-SA" are both wrong. This file records one row per list.

## The rules

1. **Bedrock bundles no filter list.** Lists are fetched at runtime, by the user's machine,
   from the list author's own URL. No Bedrock CDN, no Bedrock list proxy (invariant 1). This
   is also what keeps GPL-3.0 list *data* out of our distribution.
2. **A list may only become a default subscription when its licence has been verified against
   the list's own header or repository, and recorded below with the date.** Rows marked
   `unverified` may be offered as an optional subscription, never enabled by default.
3. **Attribution travels with the list.** CC BY / CC BY-SA lists require credit; the
   subscription UI shows the list's name, author and licence, and the "sources" view links back.
4. Removing a list from the defaults must be possible without a browser update, because a
   licence can change under us.

## Inventory

`Verified` = someone opened the list header or its repository on that date and wrote down what
it said. Everything else is a *claim from public statements* and is treated as unknown.

<!-- BEGIN FILTER LISTS -->
| List | Author | Licence (claimed) | Verified | Default? |
|---|---|---|---|---|
| EasyList | EasyList authors | CC BY-SA 3.0 / GPL-3.0 (dual) | unverified | no |
| EasyPrivacy | EasyList authors | CC BY-SA 3.0 / GPL-3.0 (dual) | unverified | no |
| EasyList Cookie List | EasyList authors | CC BY-SA 3.0 / GPL-3.0 (dual) | unverified | no |
| Fanboy's Annoyance | EasyList authors | CC BY-SA 3.0 / GPL-3.0 (dual) | unverified | no |
| uBO filters (uAssets: filters, badware, privacy, quick-fixes, unbreak) | Raymond Hill and contributors | GPL-3.0 | unverified | no |
| AdGuard Base / Tracking Protection / Annoyances | AdGuard | GPL-3.0 | unverified | no |
| Peter Lowe's Ad and tracking server list | Peter Lowe | CC BY 4.0 (claimed) | unverified | no |
| Disconnect tracking lists | Disconnect, Inc. | GPL-3.0 | unverified | no |
| URLhaus malicious URL blocklist | abuse.ch | CC0 | unverified | no |
| Steven Black unified hosts | Steven Black and contributors | MIT | unverified | no |
<!-- END FILTER LISTS -->

**Nothing above is a default today**, which is the honest state: the browser ships with an
empty default set until the verification pass happens. Earlier design text
(`docs/design/008-privacy-engine.md`) named EasyList + EasyPrivacy + uBO lists as the intended
defaults; that intent stands, and it is blocked on rule 2, not on engineering.

## Why the dual-licensed lists are not automatically safe

EasyList's dual CC BY-SA 3.0 / GPL-3.0 offer is *the recipient's choice*, which is helpful —
CC BY-SA obligations (attribution, share-alike on adaptations) are easier for us than GPL-3.0.
But two traps remain:

- **Derived data is an adaptation.** A compiled or optimised form of a CC BY-SA list that we
  distributed would be a share-alike work. Compiling happens on the user's machine, at runtime,
  from their own download — so nothing we distribute contains it. Keep it that way.
- **Aggregation is not adaptation, but bundling is distribution.** Shipping the list files
  inside our package, even unmodified, is distribution of that list under its terms —
  attribution, and for GPL-3.0 lists the corresponding-source machinery, would attach to our
  release artifacts. Runtime fetch avoids the question entirely.

## Enforcement

`scripts/check_provenance.py` checks this file on every change:

- the inventory block exists and every row is complete (list, author, licence, verified, default);
- a row marked `Default? = yes` must have a `Verified` date, not `unverified`;
- no filter-list file is committed to the tree (`*.txt` under `src_overrides/` that looks like
  filter syntax fails the build).
