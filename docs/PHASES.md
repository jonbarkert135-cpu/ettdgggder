# Implementation phases, and where Bedrock actually is

**Roadmap item 89.** Gate: `scripts/check_phases.py`. Companion: [`PROCESS.md`](PROCESS.md).

Item 89 gives the order the work should be done in. This document keeps that order next to the
honest state of each phase, because the interesting fact about this project right now is that the
two do not match — and pretending otherwise is exactly the failure mode items 55 and 88 exist to
prevent.

## Status vocabulary

| Status | Means |
| --- | --- |
| `done` | Built, tested in CI, and documented. |
| `policy-landed` | The logic and its host tests are in this repository and run on every commit, but no Chromium build performs it yet. Real, not running. |
| `not-started` | Exactly that. |

`policy-landed` is not a synonym for done. It is the honest name for what a Chromium *overlay*
can be before the overlay has ever been applied to a checkout.

## The phases

| Phase | Work | Status | Where it is |
| --- | --- | --- | --- |
| 0 | Repository, licensing, architecture | `done` | ADRs 0001–0014, `docs/LICENSING.md`, gates in CI |
| 1 | **Chromium build** | `done` | Built 2026-08-22 at the pinned revision, overlay compiled in-tree — `build/ENFORCEMENT.md` |
| 2 | **Minimal browser shell** | `done` | First Chromium call site into `bedrock::` runs in the 2026-08-23 build — `patches/bedrock/integration/0001-bedrock-startup-hook.patch`, `build/ENFORCEMENT.md` "Build 2" |
| 3 | Tabs, navigation, profiles | `policy-landed` | `ui/tab_model`, `session/`, `profiles/` + host tests |
| 4 | Search engine abstraction | `policy-landed` | `search/engine_selector`, `omnibox/input_parser`, ADR 0009 |
| 5 | Settings system | `policy-landed` | `settings/` — config surface, advanced settings, defaults, reset |
| 6 | Privacy engine | `policy-landed` | `privacy/core/`, ADR 0007, 30-feature registry |
| 7 | Content blocking | `policy-landed` | `privacy/tracker_blocker/`, ADR 0002, 0.21 µs matcher |
| 8 | Fingerprint protection | `policy-landed` | `privacy/fingerprinting/`, ADR 0008, 21 documented surfaces |
| 9 | Storage isolation | `policy-landed` | `privacy/storage/`, ADR 0011 |
| 10 | Privacy Center | `policy-landed` | `settings/privacy_center`, `settings/privacy_posture`, `ui/site_privacy_panel` |
| 11 | Theme engine | `policy-landed` | `themes/`, ADR 0012, `brand/design-tokens.json` |
| 12 | Extensions | `policy-landed` | `extensions/`, capability disclosure, catalog |
| 13 | Tor mode | `not-started` | ADR 0010 decides the shape; the daemon question is open |
| 14 | Performance optimisation | `policy-landed` | `perf/` — 6 measured metrics, 8 budgets pending a build |
| 15 | Security testing | `policy-landed` | 9 fuzz targets, 4 sanitizer configs documented, `tests/privacy/` measured against stock Chromium |
| 16 | Packaging | `not-started` | Documented in `docs/BUILD.md`, never executed |
| 17 | Documentation | `done` | `docs/README.md` index, 13 required documents, link checker |
| 18 | Release hardening | `policy-landed` | `updater/release_policy`, `scripts/verify_release.py`, ADR 0013 |

## The honest reading of that table

Phases 3–15 were built **before** phase 1, out of order. That was not an accident and it is not
free:

* **What it bought.** Every one of those subsystems is real C++ with host tests that run on every
  commit, plus documents and gates that keep the two in step. None of it is a mock. The design
  work — how blocking, fingerprinting, storage and settings fit together — is done and reviewable,
  and it is the part that is expensive to change later.
* **What it costs.** Very little of it has been proven against the engine yet. Phase 1 showed that
  all 106 overlay sources compile under Chromium's clang and reach the `chrome` link line; phase 2
  added the first real call site, so the code now *runs* — but exactly one feature
  (`webrtc_policy`) is `Status::kEnforced` and the other 29 are still policy that no build performs.
  `build/ENFORCEMENT.md` records that distinction build by build. The bill for building out of order came due immediately:
  `-fno-exceptions`, the `raw_ptr` plugin and Chromium's strict standard-library includes each
  required real changes to code that had passed its host tests for weeks.

## The owner's 16-step workflow (item 102), mapped

Item 102 states the order the project should have been built in. The phases above are that order,
renumbered; this table is the mapping, with the evidence for each claim rather than a status word.
"Policy" always means the same thing here: real, host-tested logic that no Chromium call site
invokes yet.

| Step (item 102) | State | Evidence |
| --- | --- | --- |
| 1 Research | done | [`research/`](research/) — Chromium, Brave, Firefox, Tor Browser, uBO, Privacy Badger, and `ORIGIN_TOOLS.md`, which records a project from the brief that **does not exist** rather than inventing it |
| 2 License audit / provenance map | done | [`LICENSING.md`](LICENSING.md), [`THIRD_PARTY.md`](THIRD_PARTY.md) (9 components, versions, licences, reuse mode), [`PROVENANCE.md`](PROVENANCE.md) (per file, item 91), SBOM, `check_provenance.py` |
| 3 Architecture | done | [`ARCHITECTURE.md`](ARCHITECTURE.md) incl. the trust-boundary diagram, ADRs 0001–0014 |
| 4 Threat model | done | [`security/THREAT_MODEL.md`](security/THREAT_MODEL.md) — 14 adversaries, each with where it stops |
| 5 **Chromium build** | done once, not reproducible on demand | Builds of 2026-08-22 and 2026-08-23 on Linux x64 by hand; `build/ENFORCEMENT.md`. No CI build, no Windows build, ~35–50 h on the owner's laptop ([`BUILD_ON_YOUR_MACHINE.md`](BUILD_ON_YOUR_MACHINE.md)) |
| 6 Minimal browser shell | done | First Chromium call site into `bedrock::` runs — `patches/bedrock/integration/0001-bedrock-startup-hook.patch` |
| 7 Privacy core | policy | `privacy/core/`, 30-feature registry, ADR 0007 |
| 8 Blocking | policy | `privacy/tracker_blocker/`, pipeline + CNAME uncloaking, 0.21 µs matcher |
| 9 Fingerprinting | policy, 1 enforced | 21 surfaces, letterboxing, keyed derivation; `webrtc_policy` is the single `kEnforced` feature |
| 10 UI | policy | `ui/`, own visual language ([`IDENTITY.md`](IDENTITY.md), items 96/97) — pages exist, no WebUI host registers them |
| 11 Customisation | policy | `themes/`, `branding/design-tokens.json`, ADR 0012 |
| 12 Search | policy | `search/engine_selector`, Google + DuckDuckGo with per-engine disclosure (item 93) |
| 13 Tor | not started | ADR 0010 states the open question (bundled daemon vs system Tor); no code |
| 14 Security testing | partial | 64 host tests, 9 fuzz harnesses, 31 gates, the 2026-08-25 audit and its ten findings — but no sanitiser run and no libFuzzer campaign, both of which need a build |
| 15 Performance | policy | 6 measured host-level metrics, 8 budgets that cannot be measured without a build |
| 16 Release | not started | [`BUILD.md`](BUILD.md), [`RELEASES.md`](RELEASES.md), [`REPRODUCIBILITY.md`](REPRODUCIBILITY.md) describe it; nothing has been packaged |

**The honest summary of that table:** steps 1–4 are complete and step 6 is real. Everything from
step 7 down is one build away from being either true or false, and that build is the project's only
real bottleneck — not the remaining roadmap items. Item 103 asks that every milestone leave a
working, buildable product; the tree satisfies "buildable" (the overlay compiles into Chromium and
the host suite is green on every commit) and does not yet satisfy "the privacy features work",
which is exactly why this table says `policy` eleven times instead of claiming otherwise.

## What comes next, in order

1. **Re-verify phases 3–5 against the running shell.** Phase 2 landed on 2026-08-23: the browser
   executes `bedrock::` code and the first feature (`webrtc_policy`) is `kEnforced`, proven by a
   measurement taken inside `UpdateFromSystemSettings`. The pattern is now repeatable — supply a
   default from `settings/defaults.h`, read back what the browser really uses, record the build.
   Eleven of the twelve shipped defaults are still unwired; the startup plan prints the reason for
   each one.
2. **Then the privacy phases behind them**, in the order of item 89.
3. **Phase 13 and 16**, the two remaining `not-started` product phases.

No phase between 3 and 15 may be described as done until its behaviour is observed in a running
build the same way phase 2's was — in this document, in the README, or in a release note.
`scripts/check_phases.py` enforces the vocabulary, and `scripts/check_no_fake_features.py` enforces
the consequence.
