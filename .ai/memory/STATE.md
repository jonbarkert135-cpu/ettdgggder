# Current state

Tier 1, part 2. Read straight after [`../MEMORY.md`](../MEMORY.md).
Rewritten (not appended to) at the end of every change — it describes *now*.

**As of:** build 6 (2026-08-27) added the outgoing-header floor: `URLLoader::ScheduleStart` asks
`bedrock::integration::DecideHeaders`, so a page declaring `Referrer-Policy: unsafe-url` leaks no path
to a third party and high-entropy client hints (measured: `Sec-CH-UA-Full-Version-List` to Google
origins) are dropped while the three low-entropy ones still go out — and `docs/WIRING.md` +
`scripts/report_wiring.py` now count what a running browser can reach at all: **5 of 33 modules,
8 699 of 22 670 overlay lines**, everything else compiled with tests but no call site (PR #57);
build 5 is the first build whose network behaviour
Bedrock changes: every subresource request passes through the blocking pipeline via
`integration/network_hook.h` and one call site in `services/network/url_loader_factory.cc`, and
requests that build 4 completed (Google Analytics, Facebook, DoubleClick — and three real ad hosts on
bbc.com) are now blocked with the deciding rule in the log while the sites still render; the list is
Bedrock's own 18 rules with no subscription, the engine is process-wide with shipped defaults only,
and `kPartition`/`kRedirect` verdicts still load — all of it stated in the startup line (PR #56);
build 4 extended the integration seam to typed and
scoped prefs — booleans and Local State prefs, not only profile strings — and then *withdrew* the
claim it was built for: `telemetry` and `crash_reporting` map to one Chromium consent pref, but an
unbranded build ignores that pref (`MetricsServiceAccessor::IsMetricsReportingEnabled` returns false
unless `GOOGLE_CHROME_BRANDING`), proven by flipping the value to `true` and watching the browser
keep reporting `false`; both defaults therefore stay unenforced, the log says `registering (not
decisive in this build)`, and the enforced count is still **1 of 12** (invariant 83 keeps it honest,
PR #55); `scripts/resume_build.sh` is the one command back into the build — sync, build, symbol
count, live DevTools startup check — fixed and verified end to end (PR #54); the `std::abs` defect
that build 1 found is fixed and turned into a seconds-long gate, `scripts/check_toolchain_limits.py`
(PR #53); the referrer and client-hint headers are one component — `privacy/network/request_headers`
decides both before a request leaves, a site may ask for less and never for more, and no
`Sec-CH-UA-*` identity hint goes on the wire from level 1 (PR #52, `referrer_control` policy-landed);
the build path has a laptop variant — `build/args/bedrock-lowmem.gn`, `docs/BUILD.md` → "Building on
8 GB" (PR #44); letterboxing is real geometry (PR #41); the first full security audit is on record
(`docs/security/AUDIT-2026-08-25.md`, PR #42) with F1–F10 fixed except the part of F8 needing the
profile layer; CNAME uncloaking is a stage of the blocking pipeline, cache-only by design (PR #40);
the Strict preset keeps first-party cookies and makes storage ephemeral (PR #39); link cleaning,
redirect debouncing and "forget about this site" are host-tested logic (PR #37); the interface is
written against a semantic token vocabulary enforced by `scripts/check_tokens.py` (PR #35); window
modes, the profile menu and the theme-to-CSS bridge exist (PR #34); settings, the Privacy Center and
the extensions panel have pages and tested models (PR #33); the first downloadable build is
pre-release `v0.0.1-dev` (Linux x64, PR #27).

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
| 102–104 Final workflow, no-prototype rule, three principles | done 2026-08-25 — audited the 16-step workflow against the tree instead of restarting it: steps 1–4 and 6 were already real, so the work was the honest map (`docs/PHASES.md` § "The owner's 16-step workflow"), the missing privacy architecture diagram in `ARCHITECTURE.md`, the three principles in `PROCESS.md`, and moving the threat model to `docs/security/THREAT_MODEL.md` as item 102 specifies |
| 94, 95 No hidden cloud, optional remote features | done 2026-08-25 — `privacy/network/remote_features.{h,cc}` declares all 7 permitted remote interactions (all `kPolicyOnly`, only the user's own search on by default); `scripts/check_remote_features.py` fails on networking machinery in an undeclared module, on any `bedrock.*` host anywhere, and generates `docs/privacy/REMOTE.md` |
| 92, 96, 97 Trademarks, product identity, no copied UI | done 2026-08-25 — were policy prose with no gate; now `docs/IDENTITY.md` + `scripts/check_trademarks.py` (affiliation wording, identity strings, foreign marks on disk, borrowed CSS vocabulary, undeclared influences) |
| 90–92 Quality rule, source integration, trademarks | done — items 90/91 re-audited 2026-08-25: the inventory claimed `port`/`vendored` reuse of brave-core, adblock-rust and ungoogled-chromium with **no file in the tree**; modes corrected, `docs/PROVENANCE.md` now records every third-party file (7 fields, item 91) and `check_provenance.py` ties the two together in both directions |
| 93 Search privacy disclosure | done — `onboarding/first_run` builds it from the engine facts, no search proxy |
| 94–95 No hidden cloud, optional remote features off by default | done — no compiled-in hostname, `updater` provider-abstract |
| 96–97 Product identity, own UI | done as policy — no WebUI exists to judge yet |
| 98–99 First-run flow, honest onboarding | logic + WebUI page (`ui/first_run.html`, `.js`) done and tested; the WebUI host that registers the page needs the Chromium build |
| 100–101 Continuous verification, acceptance criteria | `docs/ACCEPTANCE.md`: **11 of 31 criteria met**, rest stock/policy-only |
| Research queue: referrer + client hints | done 2026-08-26 (PR #52) — `privacy/network/request_headers`; `referrer_control` designed → policy-landed, `client_hints` gains its header layer |
| 102+ | **not yet specified — waiting on the project owner**; meanwhile the research queue in "Open threads" is worked down, newest first |

## What is real vs. what is documented

- **Exactly one feature is `Status::kEnforced`:** `webrtc_policy`. The 2026-08-23 build registers
  `webrtc.ip_handling_policy` from `settings/defaults.h` and a running browser hands
  `default_public_interface_only` to its renderers — measured inside `UpdateFromSystemSettings`,
  recorded as "Build 2" in `build/ENFORCEMENT.md`. The other 29 features are policy only.
- **The overlay runs inside Chromium** (phases 2 and 7): `nm -C out/Release/chrome | grep bedrock::`
  finds **23** symbols and `libservices_network_network_service.so` a further **51** (build 5); the
  browser prints `[bedrock]` lines at startup and one per blocked request. Phase 1 had proven
  compilation only; with no call site the linker had discarded every overlay object.
- **Current download:** GitHub pre-release `v0.0.2-dev` (2026-08-28), `bedrock-0.0.2-dev-linux-x64.tar.zst`,
  268 022 438 bytes, sha256 `e768da66ce5fabc75b96a8f446a0e6f9ed15e610f975238970e6d925bd654c83`, built from
  overlay `76a7364` by `scripts/package_release.py` (513 libraries derived via transitive `ldd`) and
  verified by unpacking and running the archive itself: blocking, header floor and the startup lines all
  behaved as in the build tree. `manifest.json` attached for `scripts/verify_release.py`.
  Notes: `docs/releases/0.0.2-dev.md`. Superseded but still published: `v0.0.1-dev`, `bedrock-0.0.1-dev-linux-x64.tar.zst`
  (313 MB, sha256 `54be5449…`), notes in `docs/releases/0.0.1-dev.md`. Component build, Linux only,
  unbranded, one enforced protection — never call it a product release. [github, 2026-08-23]
- **A local Chromium checkout and a built `chrome` exist again** [sandbox, 2026-08-27]: builds 3 and
  4 were produced there (`/work/chromium/src/out/Release`, ~194 MB binary, 17 cores via
  `os.sched_getaffinity` — `nproc` reports 1 and is wrong). An incremental overlay change costs
  2–4 minutes, a cold build ~12 hours. `scripts/resume_build.sh` is the entry point;
  `build/LOCAL_BUILD_HANDOFF.md` is the handoff. Nothing about that sandbox is permanent, so
  `docs/BUILD_ON_YOUR_MACHINE.md` still prices the machine the project needs.
- **The local build is not in git** (8.7 GB). `build/LOCAL_BUILD_HANDOFF.md` is the handoff: what
  exists on disk, what must never be rebuilt, the 11 errors hit so far, and
  `scripts/resume_build.sh` which syncs, builds and verifies in one command.
- **Runs in CI today:** 66 host test binaries, 10 fuzz smoke harnesses (~860 inputs each), 7
  measured performance metrics, **31** static gates. [run_host_tests.sh, 2026-08-27]
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
- **A pref that matches the wanted value is not proof the pref caused it.** Chromium disables
  metrics and crash upload in code in unbranded builds whatever the consent pref says, which the
  build-4 probe found only by setting the pref to the *wrong* value and rebuilding. Any future
  enforcement claim gets the same treatment.

## Open threads

- Roadmap items 90–101 were re-supplied by the owner on 2026-08-25 and re-audited
  against the tree rather than taken as done. Findings: 90/91 were overstated (see
  above); 93 (search disclosure), 98 (six-step first run, six import sources) and
  99 (five privacy notes, "protection is not invisibility") are genuinely
  implemented and host-tested, and stay `policy-only` only because no WebUI host
  registers the page. 96/97 are policy. 100/101 are the scoreboard itself —
  `docs/ACCEPTANCE.md`, 11 of 31, unchanged because nothing new was proven by a
  build.
- Host comparisons are centralised in `privacy/network/host_match.h` and fenced
  by `scripts/check_host_matching.py` (audit rec. 4). Fixing F10 there — hosts
  were compared in wire form, so `EVIL.com` and `evil.com.` bypassed every
  domain-scoped filter rule.
- **Audit debt: closed.** F1–F10 are fixed except the part of F8 that needs the
  profile layer (writing the learned table to disk — same blocker as wiring the
  features in). dns0.eu shut down in October 2025 and we shipped its preset
  anyway; presets now carry a `verified` date that expires the build after a
  year (`scripts/check_dns_presets.py`).
- `bedrock/crypto` is a *reference* implementation verified against published
  vectors. Wiring BoringSSL behind the same signatures (`BEDROCK_USE_BORINGSSL`)
  is a build-time task and is the one thing standing between this and shipping
  crypto.
- No security decision may be made by `StartsWith`/`EndsWith` on a hostname (F1);
  `scripts/check_host_matching.py` enforces it (PR #45).
- **Default filter lists are empty** until each list's licence is verified and dated in
  `docs/privacy/FILTER_LISTS.md` (item 52 rule). This is a deliberate blocker, not an oversight.
- Research queue, highest value first: dynamic filtering as a pipeline stage ·
  the blocking-`webRequest` ADR. Everything logic-only until phase 3 wires the
  entry points: referrer/Client-Hints headers (PR #52), CNAME uncloaking (#40),
  query stripping + debouncing and "forget about this site" (#37).
- No Rust module exists yet; first candidate is the filter-list parser (ADR 0004).
- From item 49, one follow-up remains: an ADR deciding whether extensions keep
  blocking `webRequest` (letterboxing landed in PR #41; "forget about this site" landed).
  Parked as too large for now: RLBox-style library sandboxing, a Bedrock root store.
- Chromium has been built **by hand on Linux only** — not in CI, never on Windows; three builds so
  far, the last on 2026-08-27 from a re-fetched checkout. The build is also driven around a siso
  scheduler stall, so objects can go stale; recompile the object of each file you edit
  (`scripts/manual_compile.py`) before linking (`scripts/manual_link.py`).
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
  C++20 std-modules all reject code that `g++` accepts. New overlay code should include what it
  uses and avoid `std::stoi`/`stod` *and* `std::abs`/`std::div` — see the table in `docs/BUILD.md`.
  `scripts/check_toolchain_limits.py` fails on all of them in seconds, so a 12-hour build is never
  again the thing that finds them (invariant 82). Host tests are exempt: they never enter the
  Chromium build.
- In this sandbox headless `--dump-dom`/`--screenshot` hang (no D-Bus, no GPU, no network) — verify
  a built binary over a DevTools port instead; `build/LOCAL_BUILD_HANDOFF.md` §5 has the recipe.
- Item 85's scoring caught four features that ship on against their own score
  (cosmetic filtering, WebGL controls, gamepad/sensors, notification gating);
  each now carries a written `default_on_reason`, and the test fails if one is
  removed. Secure DNS moved to off-by-default to match item 84's "configurable".
- Crash reporting has a policy layer and no handler: catching a real signal
  needs Crashpad from the Chromium build. Never describe item 81 as "crash
  reporting works".
