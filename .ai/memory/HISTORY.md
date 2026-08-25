# History

Newest first. One entry per merged change: what landed, and anything a future
reader would otherwise have to rediscover. Keep entries short — this file is
read, not skimmed. Anything longer belongs in a doc, linked from here.

## PR #42 — security audit of 2026-08-25

- Full audit against the owner's Project-Zero-style brief. Governing finding
  **F0**: only two patches exist (`build/0001-add-bedrock-to-chrome-browser`,
  `integration/0001-bedrock-startup-hook`) and nothing touches Blink, V8,
  //content or //net — so 20 of 21 fingerprinting surfaces and every blocking
  and storage protection are policy, not enforcement. Report:
  `docs/security/AUDIT-2026-08-25.md`.
- Two goals in the brief were rejected with reasons, not implemented: a new
  random fingerprint per launch (makes the user *more* identifiable across a
  session than a stable, common one) and a fixed 1920x1080 screen (a lie the
  first `window.resize` exposes).
- Fixed here: **F1** (critical) `IsLocalOrOnion()` matched by prefix, so
  `10.example.com` and `127.evil.test` were treated as local; **F2** (high)
  `ForNavigation()` ordering; **F5** (high) `KeysToClearForSite()` cleared the
  wrong direction of the relationship; **F6** (medium) `DnsSettings::SetStrict()`
  swallowed failures by returning void.

## PR #40 — CNAME uncloaking

- `privacy/tracker_blocker/cname_uncloak`: a DNS alias on a first-party-looking
  subdomain is re-matched against the filter lists, as stage 1b of the one
  pipeline (after the lists, before the party check). New `Reason::kCnameUncloaked`.
- **No DNS on the decision path.** `Canonical()` is cache-only; a miss queues the
  name for the embedder to resolve with the *user's* resolver, so the first
  request to an unseen aliased host still goes out. That cost is disclosed in
  the feature text, not hidden. Measured 0.175 us against a new 5 us budget.
- Uncloaking may only turn allow into block, never the reverse: a tracker cannot
  CNAME itself into a whitelisted domain. `SetUnavailable()` degrades to no
  uncloaking rather than leaking a lookup around a fail-closed DNS setting.
- `ApplyAlias()` rewrites host, eTLD+1 *and* the host inside the URL together —
  rewriting only `host` makes `||tracker^` rules silently match nothing.
- TTL clamped 60 s…24 h (the tracker publishes it), cache 4096, queue 256.
- `tracker_protection` disclosure rewritten: it used to say CNAME trackers are
  not caught at all. `docs/design/049-cname-uncloaking.md`; 30 new assertions.
## PR #39 — Strict keeps you signed in, and forgets you on close

- `security_levels`: Strict cookies `kBlock` → `kReduce` (third-party only), and
  presets now own the storage lifetime — `Isolation()` maps Strict/Maximum to
  `IsolationLevel::kEphemeralAll`, `Apply()` writes controls *and* isolation.
- Why: blocking first-party cookies makes a preset nobody can sign in on, so the
  user drops to Balanced and keeps everything persistently — strictly worse.
  The machinery already existed (item 15); only the mapping was missing.
- `Detect()` still compares controls only: `Apply()` is the single writer of the
  isolation level, so comparing it too would ask the same question twice.
- Invariant 8a; 4 new assertions; Strict's summary/tradeoff now say you are
  signed out when a site closes.


## PR #38 — what the first full build costs, and where to rent it

- `docs/BUILD_ON_YOUR_MACHINE.md`: hardware floor (16 dedicated cores, 32 GB,
  150 GB), the ~13 h build estimate derived from the 56 105-step reference run,
  hourly prices checked on the providers' own pages 2026-08-24, and the exact
  command sequence. Cheapest verified: Hetzner CCX43 at EUR 0.4423/h, so one
  full build is about EUR 7.
- Doc only; no code, no gate changes. Prices are dated in the file because they
  will drift.

## PR #37 — link cleaning, redirect debouncing, forget about this site

- `privacy/tracker_blocker/url_cleaner`: one table of 23 click identifiers plus
  a per-host redirector table. Unwraps first, then strips, ≤3 hops. Refuses
  subresources, relative / `javascript:` / `data:` / self targets and unlisted
  hosts — a generic "looks like a URL" rule would be an open-redirect engine.
  A URL with nothing to strip comes back byte-identical.
- `BlockingPipeline::CleanUrl()` now delegates; its own parameter table is gone
  (one source of truth, not two that agree today).
- `privacy/core/forget_site`: plan-then-run over 8 stores, deleters injected.
  `not available in this build` ≠ `removed`, `nothing stored` ≠ `removed`, one
  failure makes `complete()` false; passwords and bookmarks are opt-in.
- `docs/design/048-link-cleaning-and-forgetting.md`; 44 new assertions.

## PR #35 — semantic tokens, grain, background composition

- `tokens.css` gained the item 31 vocabulary (background/surface/text/border/
  accent/shadow/blur/radius), derived from the existing palette so there is one
  set of values and two names for them.
- New gate `scripts/check_tokens.py`: no hex/rgb/rgba/hsl literal in any shipped
  page, and every required semantic token must exist. Wired into run_host_tests.
- `.bedrock-canvas` is generated into tokens.css: base colour + one radial light
  source + 3.5% inline-SVG grain, both layers fixed and pointer-events:none.
- Pages moved off inline rgba() to `--highlight-hairline` and onto the canvas.

## PR #34 — settings redesign, profiles, window modes, customisation

- Settings redesign after review: type scale up one step product-wide (base 14),
  icon + 44px rows in the rail, one card per section with 72px rows, short row
  titles with the `--help` line underneath (dropped when it repeats the title).
- `themes/theme_css.{h,cc}`: the engine's state plus the window mode as a
  `:root` override over tokens.css. Clamps are the engine's, so no unclamped
  value can reach CSS; animations-off beats any duration.
- `ThemeEngine` gained surface colour, text colour, grain, shadow strength,
  glow and transition duration (design item 30's full list).
- Private/Tor windows shift the two surface colours (30% / 45% darker) and keep
  the user's accent. Each mode carries one sentence stating its limit as well as
  its benefit; a test asserts both halves.
- `ui/profile_menu.{h,cc,html,js}`: small selector, honest "Not synced" line,
  no account prompt.

## PR #33 — settings, Privacy Center, extensions

- `settings/settings_page.{h,cc}`: the settings page as a view over
  `ConfigSurface`. Every key belongs to exactly one of nine sections (asserted),
  unknown keys stay visible in Advanced, rows carry origin and policy lock.
  Extension cards travel only with the Extensions section.
- `DashboardJson()` added to `PrivacyCenter`, plus the missing
  `privacy_center_test.cc`: an empty log yields zeros, never invented numbers.
- Pages: `ui/settings.{html,js}` (left rail, segmented controls, extension
  cards) and `ui/privacy_center.{html,js}` (large type, four glass metrics).

## PR #32 — indicator, tabs, sidebar, type

- `ui/privacy_panel.{html,js}` + `PanelJson()` in `site_privacy_panel`: counts,
  states and "Not measured" stay distinguishable in the JSON and on screen.
- `TabModel` gained `TabLayout::kCompact` and `MetricsFor()`; one ordered list
  renders in all three layouts. Mockup `docs/design/mockups/tab-layouts-dark.html`.
- Type: Poppins for headings and the wordmark, Inter for UI text, both vendored
  as woff2 in `branding/fonts/` under OFL and emitted as `@font-face` by
  `gen_theme_css.py`, which now asserts the CSS contains no `https://` URL.
- Search shows a provider mark, a monogram of ours — never a third-party logo
  (item 92).

## PR #31 — new tab, search field, chrome

- `bedrock/ui/new_tab.{h,cc}`: the new tab state. Shortcut row = pinned first,
  deduplicated, capped, labelled from the host. In a private window the row is
  empty and `historyHidden` is true; bookmarks stay, because they are not
  history. Tested in `new_tab_test.cc`.
- `ui/new_tab.html` + `new_tab.js`: wordmark, one wide glass search field, fast
  shortcut row, bookmark bar. Plain WebUI, `window.bedrockSetModel(json)` in,
  `chrome.send('bedrockNewTab', [action, value])` out. No option list and no
  privacy rule in JavaScript (invariant 28).
- `docs/design/mockups/browser-window.html` moved to the token palette and got
  the full toolbar (Extensions, Profile) plus a bookmark bar and an address-bar
  focus state. `docs/design/043-new-tab-and-chrome.md` records items 11-16.
- Fixed a false negative in `check_ui_style.py`: the transition-duration regex
  matched the "." in `var(--motion-standard)` and crashed instead of checking.

## PR #30 — dark surface system

- Dark is the default theme, in the engine (`ThemeEngine::mode_`), in setup
  (`FirstRun::Choices::theme`) and in CSS. `[data-theme="system"]` is the only
  selector that follows `prefers-color-scheme`.
- New neutrals in `branding/design-tokens.json`: near-black `#0B0C0D`, graphite
  `#141517`, one glass level, borders at `#232527`/`#34373A`.
- `scripts/gen_theme_css.py` generates `src_overrides/bedrock/ui/tokens.css`
  from the tokens and fails the build when the file is stale — one palette, no
  hand-written hex in a WebUI page. Wired into `run_host_tests.sh`.
- Accent stays scarce: copper marks protection, focus and progress; the primary
  button is the brightest neutral. Surface levels and their rules are in
  `docs/design/026-visual-language.md`.

## PR #29 — the first-run page

- `onboarding/first_run_page.{h,cc}`: the whole flow state as JSON, and one
  `ApplyPageChoice(field, value)` back. Unknown field or unoffered value changes
  nothing. Strings are escaped, so a provider name cannot break the page.
- `ui/first_run.html` + `first_run.js`: plain WebUI, no framework, no network.
  It renders the model and decides nothing (invariant 28).
- Privacy level copy is read from `SecurityLevels::Info()` — one definition of
  "Balanced" (invariant 6).
- `check_ui_style.py` now also scans `src_overrides/bedrock/ui/*.html`.
- Verified by rendering the real model in a browser; the WebUI host and
  `chrome.send` routing still need the Chromium build (phase 3).

## PR #28 — Roadmap 90–101
First-run flow (`onboarding/first_run`, 6 steps, starts from the shipped
defaults so pressing through changes nothing), the item 93 search disclosure
built from the engine facts rather than a second table, and item 99's five
honest privacy notes. `docs/ACCEPTANCE.md` scores item 101: 11 of 31 criteria
met, the rest stock Chromium or policy-only. Items 90–92 and 94–97 needed no
code — the gates that enforce them already existed; the audit records which gate
covers which item.

## PR #27 — First downloadable build
Packaged the phase-2 component build as `bedrock-0.0.1-dev-linux-x64.tar.zst` (313 MB) with
`run-bedrock.sh`, README, LICENSE and THIRD_PARTY_NOTICES; published as GitHub pre-release
`v0.0.1-dev`. Repo side: `docs/releases/0.0.1-dev.md` and a download section in `README.md`.

## PR #26 — Phase 2: the overlay runs inside the browser
First Chromium call sites into `bedrock::` (`patches/bedrock/integration/0001-bedrock-startup-hook.patch`):
`RegisterBrowserUserPrefs` takes the default of `webrtc.ip_handling_policy` from the overlay and logs the
startup plan; `UpdateFromSystemSettings` logs the value the live profile hands to renderers. Rebuilt and
relinked: 17 `bedrock::` symbols in `chrome`, `[bedrock] … match` observed on a real run. `webrtc_policy`
became the **first `Status::kEnforced` feature**; `build/ENFORCEMENT.md` gained "Build 2"; `docs/PHASES.md`
phase 2 → `done`. Added `build/LOCAL_BUILD_HANDOFF.md` (11 recorded build errors + do-not-rebuild rules),
`scripts/resume_build.sh`, `scripts/manual_compile.py`, `scripts/manual_link.py`.

## PR #25 — Phase 1: Chromium built, overlay compiled in-tree
- Stock Chromium 151.0.7922.173 built from the pin: 56 105 steps, **12 h 16 m**, 194 MB binary,
  headless screenshot smoke test passed. Numbers and caveats: `build/ENFORCEMENT.md`.
- `//bedrock` wired into `//chrome/browser:browser` by
  `patches/bedrock/build/0001-add-bedrock-to-chrome-browser.patch`; 53 Bedrock objects now reach
  the `chrome` link. They are then dropped by `--gc-sections`, because **no Chromium code calls
  `bedrock::` yet** — phase 2 is what changes that. Do not claim Bedrock code runs.
- Three toolchain constraints cost the whole day and will recur: `-fno-exceptions` (no
  `std::stoi`/`stod`/`catch` — two call sites rewritten on `strtol`/`strtod`), the
  `chromium-rawptr` plugin (the target removes `find_bad_constructs`; `raw_ptr<T>` needs `//base`,
  which the overlay must not depend on), and C++20 std modules (**103 files** were relying on
  libstdc++ transitive includes; each now includes what it uses). Table in `docs/BUILD.md`.
- `scripts/gen_build_gn.py` now generates `src_overrides/bedrock/BUILD.gn` (one `source_set`, 106
  files) and runs as a gate, replacing the stale hand-written pre-item-47 file.

## PR #24 — Roadmap 82–89: transparency, defaults, trade-off scores, ADRs, process, phases
- **82 + 85** one table, `settings/knowledge/feature_disclosure`: all 30 registry features get how
  it works / what it protects / **what it cannot protect** / compatibility impact, plus five scores
  (privacy, security, compatibility loss, performance cost, complexity). Docs
  `docs/privacy/FEATURES.md` and `TRADEOFFS.md` are **generated** from it by
  `scripts/check_transparency.py --write`; the gate fails when they drift.
- The cross-check between the two tables found four real disagreements: `breaks_sites` in the
  registry disagreed with the scored compatibility loss for cross-site tracking, secure DNS,
  cookie isolation and permission isolation. Registry flags corrected; secure DNS also moved to
  **off by default** (item 84 says "configurable"), which required updating an old assertion in
  `privacy_engine_test.cc`.
- Four features ship on although `IsDefaultable()` says opt-in; each now carries a written
  `default_on_reason` and the test fails without it. `IsDefaultable` treats a zero-cost protection
  as always-on, and a compatibility loss of 3 as never-default.
- **83 + 84** `settings/defaults`: the twelve specified defaults with a rationale each, the four
  axes, and `Change` records that mark every weakening. Telemetry and crash upload are
  `negotiable = false`, and a test walks every axis × choice to prove nothing touches them.
- **86** eight new ADRs (0007 privacy architecture, 0008 fingerprinting, 0009 search, 0010 Tor,
  0011 storage, 0012 themes, 0013 updates, 0014 licensing) + `docs/adr/README.md` mapping the
  owner's ADR-001…010 numbering onto ours. `scripts/check_adr.py` required harmonising the five
  old records (missing Status line, missing sections, "Rejected alternatives" heading).
- **87–88** `docs/PROCESS.md`: ten steps before code, and the explicit stopping rules.
- **89** `docs/PHASES.md` + `scripts/check_phases.py`: all 19 phases with a status from a fixed
  vocabulary. The honest headline: phases 3–15 are `policy-landed`, phase 1 (Chromium build) is
  `not-started`, and the gate refuses to let a build-dependent phase claim `done` until
  `build/ENFORCEMENT.md` exists.

## PR #23 — Roadmap 78–81: no UI frameworks, debug logging, error handling, crash diagnostics
- **78** ADR 0006 (`docs/adr/0006-no-ui-frameworks.md`): WebUI is custom elements, shadow DOM and
  plain CSS over Chromium's own infrastructure — no React/Vue/Angular, no bundler, no npm step,
  no CDN asset. Gate `scripts/check_frameworks.py`. Note it must **not** name the two application
  shells: `check_languages.py` fails on the mere word in any file but its own.
- **79** `diagnostics/debug_log`: level `kOff` by default, two sinks (bounded memory ring, file
  *inside the profile*), `kUploadSupported = false`, and the `Sink` enum has no network member.
  Lines are scrubbed **on the way in**, so an export cannot leak what was never stored.
- **80** `errors/error_catalog`: six `BR-` codes, each with a localized title *and* action in all
  four locales (12 new message ids), and a hard split between `user_text` and `log_detail`. Five
  codes are `kDiagnosticOnly`; only the invalid-config error may show its detail, and even that is
  scrubbed first.
- **81** `diagnostics/crash_report`: `UploadConsent::kNever` default, per-report consent that also
  requires the user to have opened the report, an 11-key whitelist, 12 field names refused by name,
  frames scrubbed but source locations kept, 30-day expiry.
- Shared `diagnostics/scrubber` for all three — a redaction rule present in two of the three is the
  one that leaks. It keeps `chrome://`/`bedrock://`, loopback and source paths on purpose.
- Gate `scripts/check_diagnostics.py` (both new gates verified by breaking them: a removed
  `"cookies"` refusal, a deleted Russian error string, a `react` import).

## PR #22 — Roadmap 74–77: testing matrix, privacy regression suite, fuzzing, zero-trust deps
- **74** `tests/matrix.json` (26 required cases, each with runner + honest status) +
  `tests/MATRIX.md` + `scripts/check_test_matrix.py`. New `tests/browser/run.py`: launch,
  navigation, tabs, downloads, profiles — **5/5 against Chrome-for-Testing 151**. Three cases
  cannot run yet (extension execution, Tor proxy routing, and privacy verdicts) and say so, with
  the exact command that unblocks them.
- **75** `tests/privacy/` — 13 scenarios, 15 local fixtures, two loopback origins so cross-site is
  really cross-site, and a server that records request headers (referrer, cookies, client hints are
  not visible to JS). Stock-Chromium baseline committed as `baseline-chromium.json`: it is the
  "before" column, and it already shows `hardware_concurrency=17`, `deviceMemory=32`,
  `Sec-CH-UA-Arch: x86`, 6 fonts detected and `gclid/fbclid/msclkid` surviving navigation.
  Gate: `scripts/check_privacy_suite.py`.
- **76** five new fuzz targets — configuration parser, extension permissions, custom rules, search,
  privacy rules — for 9 total, each replayed over 860 seed inputs in CI.
- **77** `docs/THIRD_PARTY.md` grew `Reviewed` and `Justification` columns; `check_zero_trust()` in
  `scripts/check_provenance.py` rejects a review older than a year, a future date, or a
  justification made of adjectives ("popular", "nicer", "modern"). Rationale: `docs/DEPENDENCIES.md`.
- **Learned the hard way:** `--dump-dom` never returns in current new-headless, and with no network
  Chromium spends ~90 s in GCM/component-update retries before the first paint. Both runners set
  offline flags and let the page POST its own result to the local server; profile writes need ~8 s
  of settle before the process is stopped.

## PR #21 — Roadmap 70–73: supply chain, release channels, documentation set, build instructions
- **70** `docs/SUPPLY_CHAIN.md`: the chain from upstream archive to installed binary, link by link,
  with honest status per link (provenance format and signing keys are defined but **not yet
  produced** — no release exists). Trusted-source rule is four ordered checks; provenance is an
  in-toto/SLSA statement carrying the same nine values as the reproducibility manifest, so a second
  builder can compare instead of trusting. `scripts/verify_release.py` (stdlib only, offline)
  verifies manifest completeness, artifact digests, the OpenSSH detached signature via
  `ssh-keygen -Y verify`, and that the provenance is about this artifact — smoke-tested end to end
  with a real Ed25519 key.
- **71** `updater/release_channels.{h,cc,_test.cc}` + `docs/RELEASES.md` + release-notes template:
  nightly/beta/stable with cadence, soak (7/14 days), one-step promotion, and six mandatory note
  fields on every channel. Nightly may carry open blockers and an incomplete pipeline (that is what
  it is for) but may never be unsigned or undocumented.
- **72** `docs/README.md` index, new `docs/ARCHITECTURE.md` and `docs/PRIVACY.md`; THREAT_MODEL
  moved from `docs/security/` to `docs/` per the item-72 layout. `scripts/check_docs.py` checks the
  required set, resolves every relative link in 104 Markdown files, and requires the index to cover
  docs/.
- **73** `docs/BUILD.md` rewritten: prerequisites, Visual Studio component list, long-path and
  `DEPOT_TOOLS_WIN_TOOLCHAIN` setup, GN/Ninja invocations, packaging per platform, sanitizer
  configs, reproducible-release environment, and a failure table. Where a value must match the
  Chromium tree (SDK version), the document gives the command that prints it rather than a number
  that goes stale.

## PR #20 — Brand assets: the owner's logo is the mark, name is Latin in every language
- The project's own artwork (`branding/bedrock-logo.png`, plus `bedrock-logo-transparent.png` for
  icons and non-black backgrounds) is **the** mark. The generated SVG from PR #19 was removed:
  two full-size marks in one repository is one too many. `bedrock-mark-small.svg` stays, and only
  for ≤32 px, where the strata verifiably average into a grey circle.
- Name: full form **Bedrock Browser**, short form Bedrock in-product, **Latin script in every
  locale** — no Cyrillic transliteration, the same convention Firefox/Brave/Tor follow, checked by
  `check_branding.py` across catalog strings, BRAND.md and README.
- `scripts/gen_icons.py`: icons are generated (PNG 16–512, Windows .ico, Linux hicolor), not
  committed; it trims the ~15 % transparent margin first and prefers the small mark ≤24 px.
- Gate additions: PNG header check (square, alpha where the name claims it), transliteration check.

## PR #19 — Roadmap 65–69: brand identity, upstream sync, patch discipline, security priority
- **65** `docs/BRAND.md`, `branding/bedrock-mark.svg` + `bedrock-mark-small.svg`. Name unchanged
  (Bedrock, confirmed by the owner). Mark: strata of stone in a circle with one copper seam, the
  same accent the UI uses for protection state. Below 32 px the mark *changes* (three bands) rather
  than shrinking. `scripts/check_branding.py` keeps other vendors' brands out of user-visible
  strings, mockups and asset names ("Chromium" allowed as the engine, "Tor" allowed as the network,
  "Tor Browser" refused), checks docs ↔ assets both ways, and forbids restating colours outside
  design-tokens.json.
- **66/69** `updater/release_policy.{h,cc}` + `docs/UPSTREAM_SYNC.md`. Deadlines from the moment a
  fix is *public upstream*: critical 72h, high 7d, medium 14d, low 30d. Unready features are
  dropped (`kDropFeatures`), never a reason to delay. A fix touching patched code blocks the
  release until a human re-reads the patch. `kEmergencyRelease` is narrow: security-only content,
  written justification, and still the security review + privacy regression tests.
- **67** `docs/PATCHES.md`: required header (incl. `Chromium-Version` and `Drop-When`), one patch
  one purpose, `src_overrides/` preferred over a diff. Zero patches exist yet — the discipline
  lands before the first patch on purpose.
- **68** `scripts/upstream_sync.py`: `--status` (pin age, roll due), `--check-patches`,
  `--dry-run` (`git apply --check` conflict detection), `--plan`, `--selftest`.
- Gates: `check_branding.py` and `check_upstream.py` (code ↔ docs for deadlines, stages, patch
  header fields). Six negative cases verified by deliberate breakage.

## PR #18 — Roadmap 61–64: localization, platform tiers, Windows and Linux integration
- **61** `ui/l10n/string_catalog.{h,cc}`: 12 ids × 4 complete locales (en, uk, ru, de). Named
  placeholders parsed out of the text rather than declared twice; CLDR plural categories, so
  Russian and Ukrainian counted strings carry one/few/many/other while English and German carry
  two. No sentence is assembled from fragments. Fallback is locale → English only: Ukrainian never
  falls back to Russian. Unknown tags resolve to English instead of failing to start. `ui.language`
  / `--lang=` added to the config surface (15 settings now).
- **62–64** `platform/platform_support.{h,cc}`: three platforms × eleven integration points = 33
  requirements, each with an owner (Chromium-inherited vs Bedrock-owned), the requirement and the
  failure mode it prevents. Windows and Linux supported, macOS best effort *with a written reason*.
  Wayland and X11 both first class; six Linux package formats (snap deliberately not produced —
  single-vendor store). ADR 0005 records the abstraction rule.
- Gates: `scripts/check_strings.py` (locale completeness, placeholder parity, plural coverage, docs
  sync) and `scripts/check_platform.py` (platform macros only under `platform/`, no desktop
  environment named in a requirement, docs sync). Both have `--selftest`; four negative cases
  verified by breaking them on purpose.
- Docs: `docs/LOCALIZATION.md`, `docs/PLATFORMS.md`, `docs/adr/0005-platform-abstraction.md`.

## PR #17 — Roadmap 57–60: advanced settings, reset/recovery, import/export, accessibility
- **57** `settings/advanced_settings.{h,cc}`: one evaluator for custom filter lists, DNS, proxy,
  UA policy, per-site permissions/policies, CSP-like rules and managed profiles. Three verdicts —
  accepted / accepted-with-warning / rejected-with-reason — and nine named guards (G1–G9) that
  hold for enterprise policy as well as the GUI. G9 refuses the forum classics
  (`ignore-certificate-errors`, `no-sandbox`, `disable-site-isolation`) from every control.
  Warnings exist for choices that are legitimate but costly: a per-site UA override is allowed and
  is told plainly that it is *not* a privacy feature; a global one is refused.
- **58** `settings/reset_controls.{h,cc}`: the five roadmap actions, each with a changes list *and*
  an untouched list; `Confirmation::kTypeToConfirm` for the two irreversible ones, with the profile
  name typed so a confirmation cannot be given for the wrong profile, and an export offered first.
  Clear-all reuses `NewIdentity::PlanForPrivateWindowClose()` rather than retyping the target list.
- **59** `settings/portability.{h,cc}` + `docs/FORMATS.md`: five versioned formats. A newer file is
  refused, never half-read. Imports are previewed, cannot enable telemetry, forge policy, override
  a policy-locked key or point updates at plain HTTP, and every advanced value in a file goes
  through the item 57 guards. Exports never carry cookies/tokens; passwords need request *and*
  passphrase; third-party filter lists export as URLs, not contents (licences).
- **60** `ui/accessibility.{h,cc}` + `docs/ACCESSIBILITY.md`: eight requirements, each with evidence
  and a status; `Controls()` is built from the sidebar, reset and advanced tables so a nameless
  control fails the build. Destructive dialogs are alertdialogs focused on the safe button.
  `scripts/check_ui_style.py` gained the mockup rules — and both mockups needed fixing: clickable
  `<div>`s became real `<button>`s with aria-labels, decorative glyphs got `aria-hidden`, and a
  focus ring plus a `prefers-reduced-motion` block were added. Renders verified unchanged.
- Gate extension: `check_config_surface.py` now also holds guards ↔ CONFIGURATION.md and formats ↔
  FORMATS.md in sync (verified by breaking each direction).

## PR #16 — Roadmap 53–56: Privacy Badger research, "Origin Tools", no fake features, configuration
- **53** `docs/research/PRIVACY_BADGER.md`: GPL-3.0, ideas only. Queued: cookie-blocking
  (`kReduce`) as the *default* learned outcome, no learning inside private/Tor windows (a leak,
  not a nuance), one navigation-cleaning stage shared with items 50/52, and a "why was this
  flagged" view. Refused: the yellow list and the DNT compliance allowlist (unblocking on a
  promise).
- **54** `docs/research/ORIGIN_TOOLS.md`: searched on 2026-08-21 — **no such project**. Nearest
  hits recorded and rejected. Two plausible meanings answered instead: uBO's tooling (item 52),
  and Chromium **Origin Trials**, which are a real unruled fingerprinting surface — a
  recommendation is recorded, not implemented.
- **55** The registry was declared and never defined — a settings UI generated from a table that
  did not exist. Now `privacy_engine.cc` defines 30 features, each with a `Status`
  (kDesigned / kPolicyLanded / kEnforced); `UiRenderableFeatures()` returns only kEnforced, which
  is **empty today** because no Chromium build runs. `scripts/check_no_fake_features.py` fails on
  unprovable claims in user-visible copy, on thousands-separated counters not marked as sample
  data, and on any feature marked kEnforced without a `build/ENFORCEMENT.md` record. Verified by
  planting a fake "Fingerprint Protected — you are anonymous. 12,481 trackers blocked" banner.
- **56** `settings/config_surface.{h,cc}`: one table, four surfaces (GUI / config file / policy /
  CLI), precedence policy > CLI > config > GUI > default with the origin reported and policy
  values locked. Parsing is strict — unknown switch, missing value or disallowed value is an
  error, never a silent shrug. `--disable-telemetry` is accepted and cannot be inverted.
  `docs/CONFIGURATION.md` + `scripts/check_config_surface.py` keep code and manual identical.

## PR #15 — Roadmap 50–52: Brave, Tor Browser and uBlock Origin research
Three research documents in `docs/research/` in the item-49 format (mechanism → verdict →
licence → cost), plus a per-list filter-list licence inventory.
- **Brave** (`BRAVE.md`): brave-core is MPL-2.0 and MPL is per *file*, so literal reuse is
  available with a header check, a notice row and the upstream commit — never "brave-core is
  MPL". Queued: CNAME uncloaking (must use the user's resolver), query stripping + debouncing as
  one navigation-cleaning stage, referrer and Client-Hints policy, priced language reduction.
  Refused: everything needing a Brave service, and their list CDN.
- **Tor Browser** (`TOR_BROWSER.md`): the protection is the crowd, not the code — copying the
  mitigations does not copy the anonymity set, so the no-anonymity-claim rule stands. Queued:
  letterboxing, forcing HTTPS-Only inside Tor windows, circuit display, evaluating a
  JIT-disable control as attack-surface hardening separate from the privacy ladder. Confirmed
  our SOCKS `CircuitId` shape matches Tor's stream isolation.
- **uBlock Origin** (`UBLOCK_ORIGIN.md`): GPL-3.0, so syntax and documentation only. Notable:
  uBO also selects the *rarest* token — the design literature agrees with the item-46
  measurement. Queued: dynamic filtering as a pipeline stage with a "why blocked" answer,
  `!#include`/`!#if` in the parser, procedural cosmetics behind a perf budget, our own scriptlets.
- **`docs/privacy/FILTER_LISTS.md`**: one row per list, because the set is not one licence.
  A list may not be a *default* until its licence is verified and dated; nothing is default
  today (design doc 008 reconciled). `scripts/check_provenance.py` now enforces the table and
  fails if filter-list data is ever committed.

## PR #14 — Roadmap 47–49: source layout, language policy, Firefox research
Item 47: `src_overrides/bedrock/` reorganised into the proposed subsystem tree (77 files moved,
includes and header guards rewritten, zero behaviour change — all 34 tests and 4 harnesses pass
unchanged). Deviations are argued in [ADR 0003](../../docs/adr/0003-source-layout.md): no `base/`,
no `chromium/`/`app/` (the tree is fetched, not vendored), cookies folded into
`privacy/storage`, permissions left to Chromium, no top-level `tor/` (Tor is a transport, not a
subsystem), tests colocated. Namespaces deliberately unchanged — namespace names the subsystem,
path names the placement, as in Chromium.
Item 48: [ADR 0004](../../docs/adr/0004-languages.md) + `scripts/check_languages.py` (no Electron
or shipped runtime, web languages only in WebUI directories, Rust only behind `src/ffi.rs` with a
provenance row). No Rust has landed; the ADR says so instead of implying otherwise.
Item 49: `docs/research/FIREFOX.md` — 16 mechanisms with a portability verdict and the licence
position (mozilla-central is predominantly MPL-2.0, so file-level reuse with attribution is
possible; the tracker-list *data* is not). Three follow-ups recorded: letterboxing, "forget about
this site", and the blocking-`webRequest` decision (own ADR).

## PR #13 — Project memory for AI agents (`.ai/`)
Tiered memory: `.ai/MEMORY.md` + `STATE.md` restore the whole project in one
read; `MAP.md` (generated by `scripts/gen_memory.py`) routes to files without
grepping; `INVARIANTS.md`, `DECISIONS.md`, `HISTORY.md`, `PROTOCOL.md` hold the
rules, the why and the log. `scripts/gen_memory.py` regenerates the map,
`scripts/check_memory.py` fails CI when the map is stale, a code directory is
undescribed, or a change touches code/docs without updating the memory. README
gained an "AI agents start here" section; `AGENTS.md` points auto-loading agents
at it.

## PR #12 — Roadmap 43–46: fuzzing and sanitizers, threat model, security levels, performance budgets
Four libFuzzer harnesses on untrusted input (filter lists, omnibox, download
names/MIME, bookmark import), each also a deterministic CI smoke run;
ASan/UBSan, MSan, TSan and fuzz GN configs; `docs/THREAT_MODEL.md` with
all 14 adversaries and where Bedrock's protection *ends*; Standard/Balanced/
Strict/Maximum as one source of truth with Privacy Center reading it; six
measured perf metrics plus eight `pending` budgets.
**Finding:** the perf budgets caught a real bug — `FilterEngine` indexed rules by
their *longest* token, collapsing thousands of rules sharing a common word into
one bucket (70 µs matched vs. 20 µs budget, 0.35 µs unmatched). Indexing on the
*rarest* token gives 0.21 µs, ~330×, with identical match results.

## PR #11 — Roadmap 39–42
Zero-telemetry policy, provider-agnostic updates, open-source and
reproducibility gates.

## PR #10 — PrivacyTools.io ecosystem
Privacy extension store, recommendation engine, knowledge center, posture view.

## PR #9 — Roadmap 36–38
DevTools privacy panels, Privacy Center, per-site privacy panel.

## PR #8 — Roadmap 32–35
Workspaces, download manager, passwords, bookmarks and history.

## PR #7 — Roadmap 27–31
Theme engine, live customisation, tab system, sidebar, UI style gate.

## PR #6 — Roadmap 23–26
Extension disclosure, security baseline audit, central PrivacyPolicy, visual language.

## PR #5 — Roadmap 19–22
Browsing modes with Tor transport, private window, profiles, New Identity.

## PR #4 — Roadmap 15–18
Storage isolation, HTTPS policy, DNS settings, WebRTC modes.

## PR #3 — Roadmap 12–14
Filter engine, single blocking pipeline, behavioral tracker detection.

## PR #2 — Roadmap 9–11
Anti-fingerprinting levels, deterministic derivation, Protection Controller.

## PR #1 — Roadmap 6–8
Search system, omnibox classifier, Privacy Engine architecture.

## Foundation
Chromium overlay build system + licensing/provenance gate; CI.
