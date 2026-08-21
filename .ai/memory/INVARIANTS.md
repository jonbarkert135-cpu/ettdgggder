# Invariants

Rules the repository enforces on itself. Each one exists because breaking it is
easy and the damage is quiet. **Do not weaken a gate to land a change** — fix the
change, or change the gate in its own PR with a line in `DECISIONS.md`.

## Product invariants

| # | Invariant | Where it lives |
| --- | --- | --- |
| 1 | Bedrock runs no server: no backend, account, sync, telemetry, analytics, proxy or VPN | `privacy/telemetry_policy`, `scripts/check_no_telemetry.py` |
| 2 | Nothing lands without a provenance row **and** a notice file | `docs/THIRD_PARTY.md`, `scripts/check_provenance.py` |
| 3 | No uBlock Origin or Privacy Badger code (GPL-3.0 vs MPL-2.0); ideas from public docs only | `docs/THIRD_PARTY.md`, module headers |
| 4 | Exactly one component decides a block: `BlockingPipeline::Evaluate()` | `blocking/blocking_pipeline` |
| 5 | Exactly one privacy resolver: `PrivacyPolicy` (ten layers, conflicts checked) | `privacy/privacy_policy` |
| 6 | Presets are the single source of truth; Privacy Center calls `Detect()` instead of keeping a copy | `privacy/security_levels` |
| 7 | Shipped defaults equal the Balanced preset exactly | `privacy/security_levels_test` |
| 8 | The level ladder is monotone per control; every level above Standard states its cost | `privacy/security_levels_test` |
| 9 | Tor Mode is a transport mode, never a rung on the ladder, never called "anonymous" | `session/browsing_mode` |
| 10 | Anti-fingerprinting: normalize first, never random per call, every surface documented | `privacy/fingerprint_policy`, `scripts/check_fp_docs.py` |
| 11 | One StorageKey for every storage backend, cache, DNS and HSTS included | `net/storage_isolation` |
| 12 | No global certificate bypass; exceptions are per host | `net/https_policy` |
| 13 | No Chromium security mechanism is weakened for a privacy feature | `security/security_baseline` |
| 14 | An extension update can never grow the extension's powers | `extensions/extension_registry` |
| 15 | Privacy numbers come from one event log, so the surfaces cannot disagree | `stats/privacy_event_log` |
| 16 | No "restart required" for customisation — `ApplyKind` has no such value | `ui/theme_engine` |
| 17 | Deleting history deletes derived ranking data too | `data/history_store` |
| 18 | A performance claim carries a number and a unit, or it is `pending` | `scripts/check_perf_claims.py`, `docs/performance/BUDGETS.md` |

## Engineering invariants

| # | Invariant | Where it lives |
| --- | --- | --- |
| 19 | Pure logic stays dependency-free and host-testable; every code directory has a `*_test.cc` | `scripts/run_host_tests.sh`, `scripts/check_security_testing.py` |
| 20 | Builds clean under `-std=c++17 -Wall -Wextra -Werror` | `scripts/run_host_tests.sh` |
| 21 | Every fuzz harness is also compiled and replayed in CI (`-DBEDROCK_FUZZ_SMOKE`) | `src_overrides/bedrock/fuzz/` |
| 22 | Every fixed bug becomes an assertion, written as a sentence | component tests |
| 23 | Docs separate "runs in CI now" from "needs a real build" — no pretending | `docs/security/TESTING.md` |
| 24 | New code directory ⇒ entry in `.ai/memory/modules.json` | `scripts/check_memory.py` |
| 25 | Any change to code, docs or gates ⇒ project memory updated in the same PR | `scripts/check_memory.py` |
| 26 | Include paths and header guards follow the file's path (ADR 0003) | review; guards are path-derived |
| 27 | No Electron and no shipped Node/Python runtime | `scripts/check_languages.py` |
| 28 | Web languages only in WebUI directories; no privacy decision is made in TypeScript | `scripts/check_languages.py` |
| 29 | A Rust crate enters only through one `src/ffi.rs`, with no `unsafe` elsewhere and a provenance row | `scripts/check_languages.py` |
| 30 | No filter-list data is ever committed to the tree; lists are fetched at runtime by the user | `scripts/check_provenance.py` |
| 31 | A filter list becomes a default subscription only with a verified, dated licence row | `scripts/check_provenance.py`, `docs/privacy/FILTER_LISTS.md` |
