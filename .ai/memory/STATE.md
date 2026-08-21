# Current state

Tier 1, part 2. Read straight after [`../MEMORY.md`](../MEMORY.md).
Rewritten (not appended to) at the end of every change — it describes *now*.

**As of:** roadmap 70–73 merged (PR #21); brand assets finalised (PR #20).

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
| 78+ | **not yet specified — waiting on the project owner** |

## What is real vs. what is documented

- **Nothing is `Status::kEnforced`** in the feature registry, so the settings UI
  renders no protection switches yet. That is item 55 working, not a gap to paper over:
  enforcement needs a Chromium build, and `build/ENFORCEMENT.md` must record it.
- **Runs in CI today:** 48 host test binaries, 9 fuzz smoke harnesses (~860
  inputs each), 6 measured performance metrics, 22 static gates.
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

- Roadmap items 78+ awaited from the project owner.
- **Default filter lists are empty** until each list's licence is verified and dated in
  `docs/privacy/FILTER_LISTS.md` (item 52 rule). This is a deliberate blocker, not an oversight.
- Research queue, highest value first: CNAME uncloaking · query stripping + debouncing ·
  letterboxing · referrer/Client-Hints policy · dynamic filtering as a pipeline stage ·
  "forget about this site" · the blocking-`webRequest` ADR.
- No Rust module exists yet; first candidate is the filter-list parser (ADR 0004).
- From item 49, three recorded follow-ups: letterboxing, one "forget about this
  site" action, and an ADR deciding whether extensions keep blocking `webRequest`.
  Parked as too large for now: RLBox-style library sandboxing, a Bedrock root store.
- No real Chromium build has been run in CI; everything requiring one is marked
  as such in `docs/security/TESTING.md` and `docs/performance/BUDGETS.md`.
- Fuzz corpora are seed-sized only; no long campaign has run yet.
