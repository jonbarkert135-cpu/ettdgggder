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
| 1 | **Chromium build** | `not-started` | `build/chromium.pin` (151.0.7922.173), `docs/BUILD.md` — instructions exist, no build has been run |
| 2 | Minimal browser shell | `not-started` | Blocked on phase 1 |
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
* **What it costs.** Nothing has been proven against the engine. `Status::kEnforced` appears
  nowhere in the feature registry, `build/ENFORCEMENT.md` does not exist, and the settings UI
  renders no protection switches, exactly as item 55 requires. Some of the policy code will
  change on contact with Chromium's actual seams; that is the bill for the order chosen.

## What comes next, in order

1. **Phase 1: build Chromium** at the pinned revision, apply the overlay, and record the result.
   This is the single largest unblocker in the project — it converts twelve `policy-landed`
   phases into things that can be verified, turns eight performance budgets from `pending` into
   numbers, and lets `tests/browser/run.py --browser out/Release/bedrock` run against Bedrock
   instead of stock Chromium.
2. **Phase 2: the shell**, then re-verify phases 3–5 against it.
3. **Phase 13 and 16**, the two remaining `not-started` product phases.

Until step 1 happens, no phase between 3 and 15 may be described as done — in this document, in
the README, or in a release note. `scripts/check_phases.py` enforces the vocabulary, and
`scripts/check_no_fake_features.py` enforces the consequence.
