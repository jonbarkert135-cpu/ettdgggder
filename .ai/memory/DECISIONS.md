# Decisions

Why the project is shaped the way it is. One line of context, one line of
consequence — enough to avoid re-litigating a settled question, or to notice
when its premise no longer holds. Full ADRs live in `docs/adr/`.

| # | Decision | Why | Consequence |
| --- | --- | --- | --- |
| ADR 0001 | Chromium **overlay**, not a fork | A fork's diff becomes unmaintainable against Chromium's release cadence | Repo holds patches + `src_overrides/`; `build/sync.py` assembles the tree |
| ADR 0002 | One matcher interface, built-in C++ engine as default, adblock-rust as a swappable backend | Rust/FFI in the Chromium build for every platform costs more than owning the syntax long tail; the built-in engine is testable today without a checkout | `bedrock::blocking::FilterEngine` is the seam; either backend must pass the same tests |
| — | Licence: **MPL-2.0** for Bedrock's own code | Compatible with Chromium's BSD-3 base and with MPL-2.0 sources such as brave-core | GPL-3.0 sources (uBO, Privacy Badger) are idea sources only |
| — | `src_overrides/` preferred over `patches/` | A new file survives a Chromium rebase; a patch context does not | Patches are the last resort, one directory per upstream project |
| — | Pure logic is dependency-free and host-tested | CI cannot afford a 100 GB Chromium checkout, and untested logic is the default failure mode | Components avoid Chromium types in their core; `run_host_tests.sh` is the fast loop |
| — | Tor is a **transport mode**, not a security level | Putting it on the ladder implies "same but more" and implies anonymity | Separate control, separate wording, tested |
| — | Anti-fingerprinting normalizes first, noises only where normalization breaks the feature | Per-call randomness is itself a fingerprint | Deterministic derivation shared by every shim |
| — | Privacy statistics come from one event log | Three surfaces counting separately disagree within a week | DevTools, Privacy Center and the site panel all read the log |
| — | Presets are the source of truth, the UI detects rather than duplicates | A second table drifts silently | `Detect()` + apply→detect round-trip test |
| — | Zero telemetry as the shipped state, not a setting | "Anonymised telemetry" is still telemetry | Gate rejects reporting-shaped code |
| — | Performance claims need a number and a unit | "Blazing fast" cannot regress, so it cannot be defended | `check_perf_claims.py`; unmeasured budgets stay `pending` |
| — | Every gate is a script in `scripts/`, run on every PR | A rule nobody checks is a rule nobody follows | See `INVARIANTS.md` |
| 2026-08 | Project memory lives in `.ai/`, updated in the same PR as the change | Agents were re-deriving context by reading the tree, which is slow, expensive and lossy | `.ai/MEMORY.md` + `memory/`; `scripts/check_memory.py` enforces freshness |
| ADR 0003 | Subsystem tree under `src_overrides/bedrock/`, Chromium's layout where it owns the concept | Grouping by subsystem helps navigation; duplicating `base/`, permissions or site isolation fights the engine | Nested dirs; namespace names the subsystem, path names the placement; every directory needs a test and a `modules.json` line |
| ADR 0004 | C++ for engine and policy, Rust behind one FFI door, TypeScript only in WebUI, never Electron | Rust sprinkled through the browser process buys unsafe glue, not safety; a rewrite of working fuzzed C++ is not a safety win | `scripts/check_languages.py`; the first Rust candidate is the filter-list parser, if a campaign finds a bug there |
| 2026-08 (item 49) | Firefox is a *mechanism* source, per file and per licence — not a package | mozilla-central is mostly MPL-2.0 (file-level reuse possible), but its list data and vendored deps are not ours | `docs/research/FIREFOX.md`; follow-ups: letterboxing, "forget about this site", blocking `webRequest` ADR |
