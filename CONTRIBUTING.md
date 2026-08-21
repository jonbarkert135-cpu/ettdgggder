# Contributing to Bedrock Browser

Bedrock is a privacy browser, which makes two things unusual about contributing here: a change
that weakens a privacy or security property is a bug even when it works, and a change with unclear
provenance cannot be merged at all.

## Before you write code

1. **Read `docs/LICENSING.md`.** Provenance comes before code. If your change borrows from another
   project — code, an algorithm, filter data, a wordlist, an icon — it needs a row in
   `docs/THIRD_PARTY.md`, a file in `THIRD_PARTY_NOTICES/`, and a reuse mode. GPL-family code may
   only be a separate artifact or a reimplementation from public documentation.
2. **Read the design doc for the area** (`docs/design/`). Most decisions that look arbitrary are
   written down with the reason, and the reason is usually a failure mode.
3. **Check the ADRs** (`docs/adr/`) for anything structural.

## The gates

CI runs one job that must pass:

| Gate | What it protects |
| --- | --- |
| `scripts/run_host_tests.sh` | every dependency-free component test |
| `scripts/check_provenance.py` | inventory ↔ notices, pinned versions, GPL boundary |
| `scripts/check_fp_docs.py` | every fingerprinting surface has a document |
| `scripts/check_ui_style.py` | the visual limits in `docs/design/027` |
| `scripts/check_catalog.py` | per-entry licenses, attribution, verification freshness |
| `scripts/check_no_telemetry.py` | no reporting machinery, no analytics hosts |
| `scripts/generate_sbom.py --check` | the SBOM matches the inventory |

Run them all locally with `./scripts/run_host_tests.sh` before opening a pull request.

## Rules that are not negotiable

- **No telemetry.** Not aggregated, not anonymised, not "temporarily". See item 39 in
  `docs/design/041-open-source.md`.
- **No mandatory Bedrock server.** Nothing may require infrastructure we operate.
- **No anonymity claims.** A test scans user-visible strings for *anonymous*, *untraceable*,
  *100%*, *invisible* and friends. Say what a feature does, not what it makes the user.
- **No fabricated numbers.** Counters come from events that happened; "not measured" is a valid
  value and zero is a claim.
- **Do not weaken the Chromium security baseline** to add a privacy feature. If the two conflict,
  the conflict goes in the design doc before the code.

## Style

- C++17, no exceptions, no RTTI, Chromium naming. Pure logic goes in `src_overrides/bedrock/` with
  **no Chromium types**, so it compiles and runs in the host test suite.
- Every component has a `*_test.cc` with its own `main()`. Assertions describe behaviour in
  English; a test name is documentation that cannot go stale.
- Comments explain *why*, not what. If a rule exists because of a specific failure mode, name it.

## Pull requests

One roadmap item batch per branch and PR. The description says what landed, the reasoning behind
each judgement call, and what was verified. Green CI is required; a red gate is never "flaky, will
fix later".

## Security issues

Do not open a public issue. See `SECURITY.md`.
