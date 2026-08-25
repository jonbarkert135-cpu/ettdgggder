# Current state

Tier 1, part 2. Read straight after [`../MEMORY.md`](../MEMORY.md).
Rewritten (not appended to) at the end of every change — it describes *now*.

**As of:** the first full security audit is on record (`docs/security/AUDIT-2026-08-25.md`, PR #42) — four defects fixed (prefix-matched LAN detection downgrading HTTPS, shields outranking HTTPS-Only, incomplete site deletion, silently refused strict DNS), and the two cryptographic ones now fixed too (F3 envelope-encrypted master password, F4 keyed one-way seed derivation, both on `bedrock/crypto`); CNAME uncloaking is a stage of the blocking pipeline, cache-only by design (PR #40); the Strict preset keeps first-party cookies and makes storage ephemeral instead of blocking cookies outright, and presets now set the storage lifetime as well as the controls (PR #39); `docs/BUILD_ON_YOUR_MACHINE.md` prices the one full build that phase 3 needs (PR #38); link cleaning + redirect debouncing (`privacy/tracker_blocker/url_cleaner`) and "forget about this site" (`privacy/core/forget_site`) landed as host-tested logic, two items off the research queue (PR #37); the interface is written against a semantic token vocabulary, enforced by `scripts/check_tokens.py`, and the background composition (light source plus grain) is generated into tokens.css (PR #35); window modes, the profile menu and the theme-to-CSS bridge exist (PR #34); settings, the Privacy Center and the extensions panel have pages and tested models (PR #33); the privacy panel, three tab layouts and the vendored type system landed (PR #32); the new tab page, its state object and the chrome composition exist (PR #31); the dark surface system is the shipped default and tokens.css is generated from the tokens (PR #30); the first-run page renders the flow (PR #29); roadmap 90–101 audited (`docs/ACCEPTANCE.md`) and first run / search disclosure landed; phase 2 done — Bedrock code runs inside the built browser; first feature enforced (PR #26), and the first downloadable build is published as pre-release `v0.0.1-dev` (Linux x64, PR #27).

## Position on the roadmap

| Range | State |
| --- | --- |
| 1–5 Foundation: overlay build system, autonomy, licensing gate | done |
| 6–8 Search, omnibox classifier, Privacy Engine architecture | done |
| 9–11 Anti-fingerprinting levels, deterministic derivation, Protection Controller | done |
| 12–14 Filter engine, one blocking pipeline, behavioral detection | done |
| 15–18 Storage isolation, HTTPS, DNS, WebRTC | done |
| 19–22 Browsing modes + Tor transport, private window, profiles, New Identity | done |
| 23–26 Extension disclosure, security baseline, PrivacyPolicy, visual language | done |
| 27–31 Theme engine, live customisation, tabs, sidebar, UI style gate | done |
| 32–35 Workspaces, downloads, passwords, bookmarks and history | done |
| 36–38 DevTools privacy panels, Privacy Center, per-site panel | done |
| PrivacyTools.io brief: catalog, recommendations, knowledge center, posture | done |
| 39–42 Zero telemetry, provider-agnostic updates, open-source and reproducibility gates | done |
| 43–46 Fuzzing + sanitizers, threat model, security levels, performance budgets | done |
| 47 Source layout (subsystem tree, ADR 0003) | done |
| 48 Languages: C++ / Rust / TypeScript, no Electron (ADR 0004) | policy + gate done; no Rust code yet |
| 49 Firefox research (`docs/research/FIREFOX.md`) | done — analysis only, nothing implemented |
| 50–52 Brave / Tor Browser / uBlock Origin research | done — analysis + filter-list licence inventory, nothing implemented |
| 53–54 Privacy Badger research; "Origin Tools" searched and not found | done |
| 55 No fake features (registry Status + gate) | done |
| 56 Configuration system (GUI / config / policy / CLI) | done |
| 57 Advanced/enterprise settings with guards | done |
| 58 Reset & recovery surface | done |
| 59 Import/export formats | done |
| 60 Accessibility conformance surface | done |
| 61 Localization architecture | done |
| 62 Platform support tiers + abstraction | done |
| 63 Windows native integration | done |
| 64 Linux native integration | done |
| 65 Brand identity | done |
| 66 Engine version management | done |
| 67 Patch management | done |
| 68 Upstream sync tooling | done |
| 69 Security update priority | done |
| 70 Supply chain security | done |
| 71 Release engineering | done |
| 72 Documentation | done |
| 73 Build system | done |
| 74 Testing matrix | done |
| 75 Privacy regression suite | done |
| 76 Security fuzzing | done |
| 77 Zero-trust dependencies | done |
| 78 No unnecessary frameworks (ADR 0006 + gate) | done |
| 79 Local debug logging, off by default | done |
| 80 Error handling: meaningful, actionable, localized, security-conscious | done |
| 81 Local crash diagnostics, upload off by default | done — policy layer only; Crashpad needs the Chromium build |
| 82 Privacy transparency (4 statements per feature) | done |
| 83 User control: four axes, Balanced Privacy default | done |
| 84 Default settings (the 12 specified) | done |
| 85 Privacy-vs-usability scoring table | done |
| 86 ADRs (14 records, indexed and mapped) | done |
| 87–88 Research-first process, timeboxed | done — `docs/PROCESS.md` |
| 89 Implementation order | `docs/PHASES.md`: **phases 0–2 done** (builds 1 and 2 in `build/ENFORCEMENT.md`), phases 3–15 still `policy-landed` and must be re-verified against the running shell |
| 90–92 Quality rule, source integration, trademarks | done — already enforced by `check_no_fake_features`, `check_provenance`, `LICENSING.md` §4 |
| 93 Search privacy disclosure | done — `onboarding/first_run` builds it from the engine facts, no search proxy |
| 94–95 No hidden cloud, optional remote features off by default | done — no compiled-in hostname, `updater` provider-abstract |
| 96–97 Product identity, own UI | done as policy — no WebUI exists to judge yet |
| 98–99 First-run flow, honest onboarding | logic + WebUI page (`ui/first_run.html`, `.js`) done and tested; the WebUI host that registers the page needs the Chromium build |
| 100–101 Continuous verification, acceptance criteria | `docs/ACCEPTANCE.md`: **11 of 31 criteria met**, rest stock/policy-only |
| 102+ | **not yet specified — waiting on the project owner**; meanwhile the research queue in "Open threads" is worked down, newest first |

## What is real vs. what is documented

- **Exactly one feature is `Status::kEnforced`:** `webrtc_policy`. The 2026-08-23 build registers
  `webrtc.ip_handling_policy` from `settings/defaults.h` and a running browser hands
  `default_public_interface_only` to its renderers — measured inside `UpdateFromSystemSettings`,
  recorded as "Build 2" in `build/ENFORCEMENT.md`. The other 29 features are policy only.
- **The overlay runs inside Chromium** (phase 2): `nm -C out/Release/chrome | grep bedrock::` finds
  17 symbols and the browser prints `[bedrock]` lines at startup. Phase 1 had proven compilation
  only; with no call site the linker had discarded every overlay object.
- **A downloadable artifact exists:** GitHub pre-release `v0.0.1-dev`, `bedrock-0.0.1-dev-linux-x64.tar.zst`
  (313 MB, sha256 `54be5449…`), notes in `docs/releases/0.0.1-dev.md`. Component build, Linux only,
  unbranded, one enforced protection — never call it a product release. [github, 2026-08-23]
- **The local Chromium checkout and build no longer exist in any sandbox** (checked 2026-08-24),
  and the current one has 1 core. Turning a `policy` feature into an enforced one is blocked on
  hardware, not on code: see `docs/BUILD_ON_YOUR_MACHINE.md` for what to rent and what it costs.
- **The local build is not in git** (8.7 GB). `build/LOCAL_BUILD_HANDOFF.md` is the handoff: what
  exists on disk, what must never be rebuilt, the 11 errors hit so far, and
  `scripts/resume_build.sh` which syncs, builds and verifies in one command.
- **Runs in CI today:** 52 host test binaries, 9 fuzz smoke harnesses (~860
  inputs each), 7 measured performance metrics, 29 static gates.
- **Runs against a real browser binary (not in CI):**
  `tests/browser/run.py` (5/5 pass on Chrome-for-Testing 151) and
  `tests/privacy/run.py` (13 scenarios; stock-Chromium baseline committed as
  `tests/privacy/baseline-chromium.json`). Both need `--browser <path>`.
- **Tree shape since item 47:** code is nested under `src_overrides/bedrock/`
  (`privacy/{core,fingerprinting,tracker_blocker,storage,network,security,stats}`,
  plus `ui`, `themes`, `settings`, `profiles`, `workspaces`, `session`,
  `history`, `bookmarks`, `updater`, …). Namespaces were *not* renamed.
- **Requires a real Chromium build (documented, not running):** ASan/MSan/TSan
  nightlies, libFuzzer campaigns, browser and integration tests, clang-tidy,
  and 8 performance budgets marked `pending` (startup, memory, idle CPU,
  network overhead, JS benchmark). Never present these as passing.

## Last non-obvious findings

- **Filter index bug (found by the perf budgets, item 46).** `FilterEngine`
  indexed each rule by its *longest* token, so thousands of rules sharing one
  common word landed in a single bucket: 70 µs on a matching request against a
  20 µs budget, while non-matching stayed at 0.35 µs — the signature of one
  bloated bucket. Fixed by indexing on the *rarest* token (frequency counted
  over the loaded lists, ties broken by length): **0.21 µs, ~330×**, with no
  change to what matches. Lesson kept: a budget is a bug detector, not paperwork.
- The common fuzzing failure is not a missing harness but a harness that stopped
  compiling months ago — hence every harness is also a CI smoke build.
- **Driving a headless browser from a script: `--dump-dom` never returns** in
  current Chrome-for-Testing new-headless builds, and with no internet the
  browser burns ~90 s on GCM/component-update before doing anything. Both test
  runners therefore start the browser with the offline flag set in `BASE_FLAGS`
  and let the *page* POST its result to the local server. Profile writes
  (localStorage) also need ~8 s of settle time before the process is stopped,
  or they are lost.
- Defaults must equal the Balanced preset exactly, or the browser starts in a
  state its own settings call "Custom".

## Open threads

- Roadmap items 90+ awaited from the project owner.
- **Audit debt left (`docs/security/AUDIT-2026-08-25.md`):** F6b the `dns0.eu`
  preset is the website, not a DoH endpoint (a bad preset plus fallback = plaintext
  DNS) · F7 degenerate window sizes · F8 learned trackers are in-memory only ·
  F9 cert exceptions never expire. All small; none needs a build.
- `bedrock/crypto` is a *reference* implementation verified against published
  vectors. Wiring BoringSSL behind the same signatures (`BEDROCK_USE_BORINGSSL`)
  is a build-time task and is the one thing standing between this and shipping
  crypto.
- No security decision may be made by `StartsWith`/`EndsWith` on a hostname (F1);
  a gate for that pattern is not written yet.
- **Default filter lists are empty** until each list's licence is verified and dated in
  `docs/privacy/FILTER_LISTS.md` (item 52 rule). This is a deliberate blocker, not an oversight.
- Research queue, highest value first: letterboxing · referrer/Client-Hints policy ·
  dynamic filtering as a pipeline stage · the blocking-`webRequest` ADR.
  (CNAME uncloaking landed in PR #40 — logic only; the resolver plumbing is phase 3.) (Query stripping + debouncing and "forget about
  this site" are done — logic only, entry points need phase 3.)
- No Rust module exists yet; first candidate is the filter-list parser (ADR 0004).
- From item 49, two follow-ups remain: letterboxing and an ADR deciding whether
  extensions keep blocking `webRequest` ("forget about this site" landed).
  Parked as too large for now: RLBox-style library sandboxing, a Bedrock root store.
- Chromium has been built **by hand on Linux only** — not in CI, never on Windows. The build is
  also driven around a siso scheduler stall, so objects can go stale; recompile the object of each
  file you edit (`scripts/manual_compile.py`) before linking (`scripts/manual_link.py`).
  Sanitizer builds, libFuzzer campaigns and the 8 performance budgets still have no numbers;
  they stay marked pending in `docs/security/TESTING.md` and `docs/performance/BUDGETS.md`.
- Fuzz corpora are seed-sized only; no long campaign has run yet.
- No WebUI file exists yet (`settings/`, `ui/`, `devtools/` are C++ so far), so
  ADR 0006's framework ban is currently enforced against an empty set plus the
  test fixtures. It matters the day the first Settings page is written.
- **The next real unblocker is wiring the remaining 11 shipped defaults**, one at a time, using
  the phase-2 pattern: supply the default from `settings/defaults.h`, read back what the running
  browser uses, record the build. The startup plan prints the blocker for each. Twelve subsystems
  are still `policy-landed`: tested here, never run by an engine. Do not describe them as done.
- Building in-tree is not free of surprises: `-fno-exceptions`, the `chromium-rawptr` plugin and
  C++20 std-module includes all reject code that `g++` accepts. New overlay code should include
  what it uses and avoid `std::stoi`/`stod` — see the table in `docs/BUILD.md`.
- Item 85's scoring caught four features that ship on against their own score
  (cosmetic filtering, WebGL controls, gamepad/sensors, notification gating);
  each now carries a written `default_on_reason`, and the test fails if one is
  removed. Secure DNS moved to off-by-default to match item 84's "configurable".
- Crash reporting has a policy layer and no handler: catching a real signal
  needs Crashpad from the Chromium build. Never describe item 81 as "crash
  reporting works".
