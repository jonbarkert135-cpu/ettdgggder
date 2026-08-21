# Bedrock Browser — Licensing & Provenance Policy

**Status:** normative. This document is written *before* substantial code, per project rule 2.
Nothing lands in this repository until its provenance row exists in
[`THIRD_PARTY.md`](THIRD_PARTY.md) and its notice file exists in `../THIRD_PARTY_NOTICES/`.
CI enforces this (`scripts/check_provenance.py`).

## 1. Project license

Bedrock's own code (this overlay repository) is **MPL-2.0**.

Why MPL-2.0 and not BSD-3 or GPL-3.0:

- Chromium is BSD-3-Clause; BSD code can be combined with MPL-2.0 code freely.
- Brave's `brave-core` and `adblock-rust` are MPL-2.0. MPL-2.0 is **file-level copyleft**:
  a ported Brave file stays MPL-2.0 and stays open, and we can keep it in-tree legally as
  long as we preserve its header and disclose modifications. Choosing MPL-2.0 for the whole
  overlay removes any per-file license mixing question for the largest reuse source.
- Firefox and Tor Browser's Firefox patches are MPL-2.0 → same treatment.
- GPL-3.0 is **not** chosen: it would be incompatible with distributing a binary that links
  proprietary-but-permitted third-party bits Chromium already ships (e.g. some codec and
  platform SDK components), and it would make the whole browser binary GPL, which we cannot
  do while linking Chromium's ecosystem cleanly.

## 2. Reuse decision ladder

For every upstream idea or file, walk this ladder top-down and record the outcome:

| # | Mode | When it is allowed | Obligation |
|---|------|--------------------|------------|
| 1 | **Reimplement from public docs/specs** | Always. Algorithmic ideas and behaviour are not copyrightable. | Cite the inspiration in the source header; no license inherited. |
| 2 | **Port / adapt source (MPL-2.0, BSD, Apache-2.0, MIT)** | License is compatible with MPL-2.0 distribution. | Keep original header, add "Modified by the Bedrock authors", row in `THIRD_PARTY.md`, notice file. |
| 3 | **Vendor unmodified (any OSI license compatible with §1)** | Library used as-is. | Pin exact tag/commit, notice file, no header edits. |
| 4 | **Ship as a separate, independently-distributed artifact** | Source is GPL-3.0 / AGPL and would infect the browser binary (uBlock Origin, Privacy Badger). | Distribute as its own package with its own full license text; **no linking, no vendoring into the binary**. |
| 5 | **Do not use** | License unclear (NOASSERTION without a resolvable LICENSE file), non-commercial, or trademark-encumbered assets. | Record the refusal and the reason. |

"X is open source, therefore copy X" is a policy violation and grounds for reverting a change.

## 3. GPL-3.0 boundary (the critical case)

**uBlock Origin (GPL-3.0)** and **Privacy Badger (GPL-3.0)** are the two sources the master
prompt names for content blocking and tracker protection. Neither may be vendored into or
linked with the Bedrock binary.

Permitted paths:

- **Filter *lists*** (EasyList, EasyPrivacy, uBO's own lists) are data, licensed **CC BY-SA 3.0 /
  GPL-3.0 depending on list**. They are downloaded at runtime by the user's browser or shipped
  as a separate data package, never compiled in. Attribution required for CC BY-SA lists.
- **Filter *syntax*** (ABP/uBO syntax) is a documented format → reimplement a parser (ladder #1).
- **Engine:** reimplement the documented ABP/uBO filter syntax (ladder #4, shipped as
  `bedrock::blocking::FilterEngine`), with Brave's `adblock-rust` (MPL-2.0, ladder #2/#3) as a
  swappable backend — see ADR 0002. Never port uBO's engine.
  It already implements the uBO/ABP syntax and cosmetic filtering, and its license fits.
- **Privacy Badger's heuristic** (flag a third party seen tracking on ≥3 sites) is a published
  algorithm → reimplement (ladder #1). Do not copy its code, list files, or its "yellow list".
- If the user wants the real extensions, Bedrock loads them as ordinary WebExtensions the user
  installs — that is aggregation, not a derivative work.

## 4. Trademarks

Trademarks are **not** granted by any of these licenses.

- `Chrome`, `Chromium`, `Google` branding, the Google APIs keys, and the Chrome Web Store
  branding must be removed/replaced. Bedrock ships its own name, logo (`branding/`) and
  `.gn` branding block.
- `Brave`, `Firefox`, `Tor`, `uBlock Origin`, `Privacy Badger` names may only be used
  descriptively ("filter syntax compatible with uBlock Origin"), never as our own branding
  and never in a way implying endorsement.
- Tor: the Tor Project explicitly restricts use of the name for modified builds. Bedrock does
  **not** call anything "Tor mode".

## 5. Required record per dependency

Every row in `THIRD_PARTY.md` carries: project, repository, exact tag/commit, source files
used, original copyright, license (SPDX), reuse mode (from §2), attribution, redistribution
obligation, source-disclosure obligation, trademark note, compatibility note.

## 6. Source disclosure

- MPL-2.0 files: we must offer the source of **those files** (including our modifications).
  Satisfied by this repository being public.
- BSD/MIT/Apache: notice + license text in the shipped `about:credits` and in the release
  archive (`THIRD_PARTY_NOTICES/`).
- Release builds must regenerate `about:credits` from Chromium's own
  `tools/licenses/licenses.py` **plus** our `THIRD_PARTY_NOTICES/`.

## 7. Enforcement

`scripts/check_provenance.py` fails CI when:

1. a notice file exists with no row in `THIRD_PARTY.md`, or vice-versa;
2. a row has an unpinned version (`main`, `master`, `latest`, empty);
3. a row uses a GPL-family license with a reuse mode other than `separate-artifact` or `not-used`;
4. the Chromium pin in `build/chromium.pin` is malformed.
5. `scripts/check_catalog.py` fails CI when a catalog entry lacks its own license, official
   source or attribution, when a third-party extension is presented as a Bedrock one, when a
   PrivacyTools.io recommendation does not credit PrivacyTools.io, or when an entry's
   `last_verified` date is missing, in the future, or older than 180 days.

## 8. Curated content and recommendations (PrivacyTools.io)

Recommending a tool is not the same as shipping it, but it still carries obligations.

- **A site's license covers that site's own work.** PrivacyTools.io publishes under the VERNAM
  License — permissive, with one binding condition: a clear, visible, working credit link back to
  https://www.privacytools.io, and no implication of endorsement. Their names and marks are not
  granted with it. That condition is met in the extension store and Knowledge Center headers, and
  the catalog gate fails the build if the attribution string disappears.
- **It does not cover the tools they link to.** uBlock Origin stays GPL-3.0, ClearURLs stays
  LGPL-3.0, Cookie AutoDelete stays MIT, Decentraleyes stays MPL-2.0. Every catalog entry
  therefore carries its own `license`, `official_source` and `attribution`. Treating a curator's
  license as covering everything it mentions is the exact mistake this section exists to prevent.
- **Concept vs. text.** The Covered / Hardened / Targeted model is used as a *concept*, with
  Bedrock's own wording. Copying their prose or branding would need more than a credit link — it
  would need us to be honest that it is theirs, which a browser UI is a poor place to do.
- **Layer A / Layer B** (`docs/design/040`): Bedrock only stores an article locally when its
  provenance record says redistribution is allowed. Everything else is a link card with a source
  badge. `KnowledgeBase::Add()` refuses the alternative, so "we will fix the licensing later"
  cannot ship.
