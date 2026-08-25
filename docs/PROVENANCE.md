# Provenance — every piece of third-party material, file by file

**Roadmap item 91.** [`THIRD_PARTY.md`](THIRD_PARTY.md) answers *which projects* Bedrock relates to.
This file answers the question an auditor, a distribution maintainer or a curious user actually
asks: **for this file in the tree, where exactly did it come from, and was it changed?**

Item 91 requires seven fields per piece of third-party material — source project, licence,
version, commit, file, modification status, attribution — and requires that all of it be
traceable. Machine-checked by `scripts/check_provenance.py`, in both directions:

1. every record below names a project that exists in the inventory, with the same licence, an
   existing file in this tree, a pinned commit or dated snapshot, and a modification status;
2. every inventory row whose reuse mode is `port` or `vendored` has at least one record here —
   a claim of code reuse with nothing to point at is not traceability, it is intent;
3. a source file that declares `Derived-from:` in its header must have a record, and its project
   must be one the inventory permits code from.

Modification status is one of **verbatim** (byte-identical), **modified** (changed; the change is
in our history and the file keeps its original header plus a modification line), **data-snapshot**
(their data, curated by us, no executable code).

<!-- BEGIN PROVENANCE -->
| File in this tree | Source project | Licence | Version | Commit / snapshot | Upstream file | Modification status | Attribution |
|---|---|---|---|---|---|---|---|
| `src_overrides/bedrock/extensions/catalog/bedrock_privacy_catalog.json` | PrivacyTools.io | VERNAM License | catalog-snapshot-2026-08-21 | snapshot-2026-08-21 | https://www.privacytools.io/privacy-browser-addons | data-snapshot | Credit link to https://www.privacytools.io in the extension store and Knowledge Center, enforced by `scripts/check_catalog.py` |
<!-- END PROVENANCE -->

## Why the table is one row long

Because that is the truth of this tree today, and item 90 forbids implying otherwise. Bedrock is a
5 MB overlay of independently written C++ on a Chromium base. **No file in `src_overrides/` is
derived from another browser's source.** The privacy mechanisms were written from public
descriptions and specifications — the research notes in [`research/`](research/) record which
description each one came from — and the mechanism-level verdicts in
[`research/BRAVE.md`](research/BRAVE.md) exist precisely so that reading Brave's *behaviour* never
turns into copying Brave's *files* without a record.

This was corrected on 2026-08-25 (roadmap items 90 and 91). The inventory previously listed
`brave-core` and `ungoogled-chromium` as reuse mode `port` and `adblock-rust` as `vendored`, and
`THIRD_PARTY.md` described how ported files "keep their MPL-2.0 header … with the exact upstream
path and commit" — a process with **no instances**. Nothing was mis-licensed and nothing was
copied; the record simply described an intention as if it were the state of the tree, which is the
same failure item 90 bans in a feature switch. The modes now match reality (`reimplement`,
`not-used`), and the gate makes the two statements move together: adopting a file from any of
those projects means adding a record here *and* upgrading the mode, in the same commit, or CI
fails.

## Chromium

The base is not a record here, because it is not a chunk of copied code: it is fetched, unmodified,
by `build/sync.py` at the pin in [`../build/chromium.pin`](../build/chromium.pin) — version and
commit hash — and every change to it is a patch in [`../patches/`](../patches/), listed in
[`PATCHES.md`](PATCHES.md). That is the same seven fields, kept in the form a build can act on.

## Adding a record

If you bring in third-party code:

1. Keep the original licence header. Add one line: `Modified by the Bedrock authors, <what>`.
2. Add `Derived-from: <project> <upstream path> @ <commit>` to the file header. The gate looks
   for this and will demand the rest.
3. Add the row here, and the notice file under `THIRD_PARTY_NOTICES/`.
4. Set the inventory reuse mode to `port` (individual files) or `vendored` (a whole component).
5. Check the licence is compatible **for the reuse mode you chose** — GPL-family code may only be
   `separate-artifact` or `not-used`, and `check_provenance.py` enforces it.

If any of that feels like too much work for the code in question, that is the check working:
rewriting a small mechanism from its public description is usually cheaper than owning someone
else's file forever.
