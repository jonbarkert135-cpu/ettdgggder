# Acceptance criteria (roadmap item 101)

Item 101 lists what must be true before the project may be called ready. This
file is the honest scoreboard: one row per criterion, and a status that may only
be `yes` when something outside this document proves it — a build recorded in
[`../build/ENFORCEMENT.md`](../build/ENFORCEMENT.md), a test runner, or a gate in
`scripts/`. Host tests prove the *logic*; they do not prove a browser does it.

Statuses: **yes** (proven, with the proof named) · **stock** (works because it is
Chromium's, Bedrock's own version is not wired in) · **policy-only** (specified,
host-tested, no engine executes it) · **no**.

Last updated against build 2 (2026-08-23, Linux x64, `is_component_build=true`); test and
gate counts refreshed 2026-08-25.

| # | Criterion | Status | Proof / what is missing |
| --- | --- | --- | --- |
| 1 | Browser builds | yes | `build/ENFORCEMENT.md` builds 1–2; Linux x64 only, never built on Windows or in CI |
| 2 | Browser launches | yes | build 2 starts and prints `[bedrock]` lines; the shipped archive was launched outside the build tree |
| 3 | Tabs work | stock | Chromium's tabs; `ui/tab_model` is policy-only |
| 4 | Navigation works | stock | — |
| 5 | Google works | stock | Bedrock's provider list is not loaded by the build yet |
| 6 | DuckDuckGo works | stock | as above; DuckDuckGo is the intended default |
| 7 | Profiles work | stock | `profiles/profile_manager` policy-only |
| 8 | Private mode works | stock | Bedrock's browsing modes and New Identity policy-only |
| 9 | Extensions work | stock | disclosure layer policy-only |
| 10 | Content blocking works | policy-only | filter engine is host-tested and fast (0.21 µs); **the default lists are empty by design** until each licence is verified (`privacy/FILTER_LISTS.md`) |
| 11 | Tracker blocking works | policy-only | `tracker_blocker/*` host-tested, not wired |
| 12 | Fingerprinting protection works | policy-only | `fingerprinting/fingerprint_policy` host-tested, not wired |
| 13 | Storage isolation works | policy-only | — |
| 14 | HTTPS protection works | policy-only | — |
| 15 | Privacy Center reports real data | no | no WebUI exists; the event log has no producer in the engine |
| 16 | Theme system works | policy-only | — |
| 17 | UI customization works | policy-only | — |
| 18 | Settings persist | policy-only | no pref registration beyond `webrtc.ip_handling_policy` |
| 19 | Downloads work | stock | — |
| 20 | Bookmarks work | stock | — |
| 21 | History works | stock | — |
| 22 | Security sandbox enabled | yes | release args keep the Chromium sandbox; `privacy/security/security_baseline` fails any change that weakens it |
| 23 | No mandatory backend | yes | no hostname is compiled in; `updater` is provider-abstract, tested for it |
| 24 | No hidden telemetry | yes | `check_no_telemetry.py`: no reporting machinery, `enable_reporting=false`, `safe_browsing_mode=0`, `use_official_google_api_keys=false` |
| 25 | Licensing documented | yes | `LICENSING.md` + `check_provenance.py` (9 dependencies, 9 notices, per-file records) |
| 26 | Third-party components documented | yes | `THIRD_PARTY.md` (projects) + `PROVENANCE.md` (files, item 91), both machine-checked against each other |
| 27 | Tests pass | yes, for what they cover | 64 host tests, 9 fuzz smoke harnesses, 31 gates in CI; sanitizers, libFuzzer campaigns and browser tests need a Chromium build and have no numbers |
| 28 | Build instructions work | yes, on Linux | `BUILD.md` + `scripts/resume_build.sh` reproduce build 3 (2026-08-27); Windows instructions are untested |
| 29 | Security documentation exists | yes | `SECURITY.md`, `docs/security/` |
| 30 | Threat model exists | yes | `docs/security/THREAT_MODEL.md` |
| 31 | Upstream strategy exists | yes | `docs/UPSTREAM_SYNC.md`, `PATCHES.md`, `scripts/upstream_sync.py` |

**The honest summary: 11 of 31 criteria are met.** Everything in the privacy
product itself is `policy-only` or `stock` until the remaining shipped defaults
and subsystems are wired into a running engine, one at a time, the way
`webrtc_policy` was. Nothing in this table may be upgraded because it "should
work" — only because a named build, test run or gate says it does.
