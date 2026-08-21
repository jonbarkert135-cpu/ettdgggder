# History

Newest first. One entry per merged change: what landed, and anything a future
reader would otherwise have to rediscover. Keep entries short — this file is
read, not skimmed. Anything longer belongs in a doc, linked from here.

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
ASan/UBSan, MSan, TSan and fuzz GN configs; `docs/security/THREAT_MODEL.md` with
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
