# ADR 0014 — MPL-2.0 for Bedrock's own code, and a boundary the GPL cannot cross by accident

**Status:** accepted (2026-08-21) · roadmap items 4, 5, 50–53 · owner's list: ADR-010
**Documents:** `docs/LICENSING.md`, `docs/THIRD_PARTY.md`, `NOTICE`

## Context

Bedrock stands on code under at least four licence families: Chromium (BSD-3-Clause plus a long
tail), Brave components (MPL-2.0), uBlock Origin (GPL-3.0), Privacy Badger (GPL-3.0-or-later),
and filter lists whose own terms are separate from the code that reads them. Item 5 requires the
project to be legally publishable as open source; items 50–53 require studying GPL projects
without copying them.

Licence mistakes in this area are not fixable by a later commit: once GPL code is linked into a
non-GPL binary and shipped, the remedy is a relicense or a removal, not an apology.

## Decision

1. **Bedrock's own code is MPL-2.0.** File-level copyleft: modifications to Bedrock files come
   back, while the file boundary keeps the combined work with Chromium's BSD code coherent. Same
   licence Brave and Firefox chose, for the same reason.
2. **GPL-licensed projects are studied, never linked.** uBlock Origin's filter *syntax* is public
   documentation and reimplementable; its code is not usable here. Every reimplementation records
   what was read and what was written from scratch (`docs/research/`).
3. **A component keeps its own licence, in its own directory, with attribution.** Anything reused
   verbatim lives under a directory carrying the original LICENSE and NOTICE text; nothing is
   relicensed by moving it.
4. **Filter lists are data with separate terms.** A list is not shipped until its licence is
   verified and dated in `docs/privacy/FILTER_LISTS.md`. Default lists are empty today for exactly
   this reason — a deliberate blocker, not an oversight.
5. **The gate runs on every commit.** `scripts/check_open_source.py` and
   `scripts/check_provenance.py` fail the build on a file without a licence header, a dependency
   without a provenance row, or a component whose licence is incompatible with distribution.

## Alternatives considered

* **BSD-3-Clause**, matching Chromium. Rejected: it permits a closed fork of Bedrock's privacy
  work, which is the one thing this project exists to prevent.
* **GPL-3.0** for everything. Rejected: incompatible with distributing a Chromium-derived binary
  and with the reuse of MPL components.

## Consequences

* Some attractive code is off limits. That is the cost of being publishable, and it is cheaper
  than the alternative.
* Every new dependency needs a licence, a dated review and a justification before it lands
  (item 77, `docs/DEPENDENCIES.md`).
