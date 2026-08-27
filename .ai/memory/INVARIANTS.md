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
| 8a | From Strict up, first-party cookies keep working but storage is erased when the site closes — a preset sets the lifetime, not only the controls | `privacy/security_levels_test` |
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
| 32 | The UI renders a protection control only for features the browser enforces | `privacy/core/privacy_engine` (`UiRenderableFeatures`), `privacy_engine_test` |
| 33 | No unprovable claim and no unbacked counter in user-visible copy | `scripts/check_no_fake_features.py` |
| 34 | Every major control is reachable from GUI, config file, policy and (where useful) CLI, or states why not | `settings/config_surface`, `config_surface_test` |
| 35 | An unknown switch or invalid value is an error, never silently ignored | `settings/config_surface`, `config_surface_test` |
| 36 | Every CLI switch is documented, and every documented switch exists | `scripts/check_config_surface.py` |
| 37 | No advanced setting or policy can disable cert validation, the sandbox or site isolation | `settings/advanced_settings` (G9), `advanced_settings_test` |
| 38 | An advanced input is accepted, warned about, or refused with a reason — never silently dropped | `settings/advanced_settings`, `advanced_settings_test` |
| 39 | Every reset action states what it does *not* touch, and irreversible ones need the profile name typed | `settings/reset_controls`, `reset_controls_test` |
| 40 | An import can lower privilege but never raise it; a newer file version is refused, not half-read | `settings/portability`, `portability_test` |
| 41 | Exports carry no secrets unless explicitly requested and encrypted, and never third-party list contents | `settings/portability`, `docs/FORMATS.md` |
| 42 | Every Bedrock control has a keyboard path and an accessible name | `ui/accessibility`, `accessibility_test`, `scripts/check_ui_style.py` |
| 50 | A Chromium security fix outranks every feature; features are dropped, the release is not delayed | `updater/release_policy`, `release_policy_test`, `docs/UPSTREAM_SYNC.md` |
| 51 | No build reaches users without the security review and the privacy regression tests — emergencies included | `release_policy` (`MandatoryStages`), `scripts/check_upstream.py` |
| 52 | Every patch states a Reason and a Drop-When, and is verified against the pinned Chromium version | `docs/PATCHES.md`, `scripts/upstream_sync.py --check-patches` |
| 53 | No other vendor's brand appears in a user-visible string, mockup or asset name; the product name is Latin script in every language, never transliterated | `scripts/check_branding.py` |
| 54 | Colours are defined only in `branding/design-tokens.json`; documents name them, never restate them | `scripts/check_branding.py` |
| 55 | Every release states all six fields — version, Chromium base, security fixes, privacy changes, dependencies, known issues — on every channel including nightly | `release_channels`, `scripts/check_releases.py` |
| 56 | No build is published unsigned or without provenance, on any channel | `release_channels` (`kUnsigned`), `docs/SUPPLY_CHAIN.md` |
| 57 | Promotion is nightly → beta → stable, one step, after the channel's soak | `release_channels` (`IsPromotionAllowed`) |
| 58 | Every required document exists and every relative link in the repository resolves | `scripts/check_docs.py` |
| 44 | No user-visible string is written at its display site; it comes from the catalog by id | `ui/l10n/string_catalog`, `scripts/check_strings.py` |
| 45 | A locale is offered only when every id is translated; placeholders match the English source | `scripts/check_strings.py`, `string_catalog_test` |
| 46 | Ukrainian never falls back to Russian — only to English | `string_catalog` (`FallbackChain`), `string_catalog_test` |
| 47 | Platform macros appear only under `src_overrides/bedrock/platform/` | `scripts/check_platform.py` |
| 48 | No Linux code path assumes a desktop environment; Wayland and X11 are both first class | `platform/platform_support`, `scripts/check_platform.py` |
| 49 | A platform is called supported only when it is built, tested and released | `platform_support` (tiers + reasons), `platform_support_test` |
| 43 | A destructive dialog is an alertdialog that opens focused on the safe choice | `ui/accessibility` (`ContractFor`), `accessibility_test` |
| 59 | Every case in roadmap item 74's matrix exists in `tests/matrix.json` with a runner that exists, and `running` is claimed only by something that really executes | `scripts/check_test_matrix.py` |
| 60 | Privacy tests use local fixtures only; no fixture may reference an off-machine URL, and no measurement leaves the machine | `scripts/check_privacy_suite.py`, `tests/privacy/` |
| 61 | A claim that Bedrock changes a browser-observable value is comparable against a recorded stock-Chromium measurement | `tests/privacy/baseline-chromium.json` |
| 62 | A dependency needs a dated review and a justification about what breaks without it; "looks nicer" is rejected | `scripts/check_provenance.py` (`check_zero_trust`), `docs/DEPENDENCIES.md` |
| 63 | No JS framework, bundler, Node manifest or off-machine asset anywhere in the tree; WebUI uses the platform | `scripts/check_frameworks.py`, ADR 0006 |
| 64 | Debug logging is off by default and has no sink that can reach the network; lines are scrubbed before they are stored | `diagnostics/debug_log`, `scripts/check_diagnostics.py` |
| 65 | Crash upload defaults to never, needs per-report consent, and a report carries whitelisted fields only — never URLs, cookies, credentials or the profile path | `diagnostics/crash_report`, `crash_report_test` |
| 66 | Every error has a localized title *and* an action in all four locales, and its internal detail goes to the log, not the screen | `errors/error_catalog`, `scripts/check_diagnostics.py` |
| 67 | Every privacy feature states how it works, what it protects, what it cannot protect and what it breaks | `settings/knowledge/feature_disclosure`, `scripts/check_transparency.py` |
| 68 | `breaks_sites` in the feature registry and the scored compatibility loss must agree | `feature_disclosure_test` |
| 69 | A feature shipped on against its own trade-off score carries a written argument | `feature_disclosure_test` (`default_on_reason`) |
| 70 | The twelve item-84 defaults ship as specified; telemetry and crash upload are on no axis of user control | `settings/defaults`, `scripts/check_defaults.py` |
| 71 | Every ADR has context, decision, alternatives and consequences, and is in the index | `scripts/check_adr.py` |
| 72 | A phase that needs a Chromium build cannot be called done until `build/ENFORCEMENT.md` records one | `scripts/check_phases.py`, `docs/PHASES.md` |
| 73 | Fingerprint seeds are derived with a keyed one-way function, never a reversible mixer | `fingerprint_policy_test` (surface-key assertions) |
| 74 | A password or key is verified by an AEAD tag, never by comparing stored ciphertexts | `password_store_test` (master-password assertions) |
| 75 | A host name is compared only via `privacy/network/host_match.h` — normalised, at label boundaries, addresses parsed | `scripts/check_host_matching.py`, `host_match_test` |
| 76 | Every shipped DNS preset is a real endpoint with a named operator and a re-check date under a year old | `scripts/check_dns_presets.py`, `dns_settings_test` |
| 77 | Security exceptions expire; learned evidence ages out; user decisions do neither | `https_policy_test` (F9), `tracker_heuristic_test` (F8) |
| 78 | A reuse mode claiming third-party material in the tree has a per-file provenance record, and vice versa | `scripts/check_provenance.py` rule 8 |
| 79 | Another vendor's name may describe their product, never ours, and never near words implying endorsement; no foreign mark or CSS vocabulary in the tree | `scripts/check_trademarks.py` |
| 80 | Every research note under `docs/research/` has a stance in `docs/IDENTITY.md` | `scripts/check_trademarks.py` |
| 81 | Networking machinery only in a module declared in `remote_features.cc`; no host under our own name anywhere in the tree | `scripts/check_remote_features.py` |
| 82 | In-tree overlay code uses no exceptions and no `<cstdlib>` numeric helper — what `g++` accepts is not what Chromium's clang accepts | `scripts/check_toolchain_limits.py` |
| 83 | A pref Bedrock registers but does not decide the behaviour with is listed as unenforced and logged as `registering`, never `enforcing` | `startup_test` (`RegisteredButNotDecisiveIsAlsoUnenforced`), `build/ENFORCEMENT.md` build 4 |
| 84 | Whether a network request is made is decided in one place — `bedrock::integration::DecideRequest`; a request whose top-level document is unknown is never blocked, and the Chromium patch performs the verdict without adding policy | `network_hook_test`, `patches/bedrock/integration/0003-*` |
| 85 | The outgoing-header floor may only shorten what Chromium was going to send — never lengthen a referrer, never add a hint, never return a header that was not present | `network_hook_test` (`TheDecisionNeverLengthensAReferrer`, `OnlyPresentHeadersAreEverReturned`) |
