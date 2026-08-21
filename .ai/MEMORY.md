# Bedrock Browser — project memory (tier 1)

**If you are an AI agent picking this project up: read this file completely, then
[`memory/STATE.md`](memory/STATE.md). That is the whole restore. ~4k tokens, no
source files, no `git log`.** Everything else is opened on demand via
[`memory/MAP.md`](memory/MAP.md).

Last verified against: commit of PR #12 (roadmap 43–46).

---

## 1. What the project is

A **production-grade, fully autonomous, open-source desktop browser built on
Chromium** — not a demo UI, not an Electron app, not a WebView wrapper, not a
re-skin. It must really build, really run, and be maintainable as an independent
project.

This repository is the **overlay**, not a Chromium fork: patches, new source
files, build args, branding, tooling. `build/sync.py` fetches the pinned
Chromium tree and applies them ([ADR 0001](../docs/adr/0001-chromium-overlay.md)).

**Design sources — study, never blind-copy:** Chromium (foundation), Brave
(privacy mechanisms), Firefox (architecture), Tor Browser (anti-fingerprinting
thinking), uBlock Origin (content blocking), Privacy Badger (tracker
protection). For each: decide what is legally reusable, what is integrated as a
separate component under its own licence, what is reimplemented from public
documentation, and what is off-limits — recorded in
[`docs/THIRD_PARTY.md`](../docs/THIRD_PARTY.md) before any code lands.

## 2. Non-negotiables (breaking one is a bug, not a trade-off)

1. **Autonomous.** Bedrock operates **no server of any kind** — no backend,
   account, sync, telemetry, analytics, proxy or VPN. The browser talks only to
   the sites, search engine and resolver the *user* chose.
2. **Zero telemetry**, shipped state, not a setting to find (item 39).
3. **Licensing first.** No code without a provenance row and a notice file.
   Hard GPL-3.0 boundary around uBlock Origin and Privacy Badger: their ideas
   and public docs are used, their code is not. Bedrock's own code is MPL-2.0.
4. **One source of truth per concern.** `PrivacyPolicy` for privacy layers,
   `BlockingPipeline::Evaluate()` for blocking, `security_levels` for presets,
   `privacy_event_log` for counters. A second table is a bug even when it agrees
   today.
5. **Honesty over marketing.** No "anonymous", no "blazing fast", no untestable
   claim. Anti-fingerprinting raises cost, it does not make one indistinguishable
   fingerprint. Tor Mode is a *transport mode*, deliberately not a rung on the
   security-level ladder. Unmeasured budgets are labelled `pending`.
6. **Never weaken a Chromium security mechanism** to make a privacy feature
   easier (enforced by `security/security_baseline`).
7. **Gates are not obstacles.** If a CI gate blocks a change, fix the change.
   Weakening a gate needs its own PR and a line in `DECISIONS.md`.

## 3. How the code is organised

```
build/          chromium.pin, sync.py, args/*.gn (release, debug, asan, msan, tsan, fuzz)
patches/        patches against the Chromium tree — last resort
src_overrides/  new files in Chromium tree layout — the preferred way to add code
docs/           LICENSING, THIRD_PARTY, BUILD, adr/, design/, privacy/, security/, performance/
scripts/        the CI gates + run_host_tests.sh
THIRD_PARTY_NOTICES/  one notice per dependency, 1:1 with the inventory
```

Pure logic is written **dependency-free and host-testable**: every component
ships `*_test.cc` with its own `main()`, built by `./scripts/run_host_tests.sh`
with plain `g++ -std=c++17 -Wall -Wextra -Werror`. No Chromium checkout needed
for CI. Keep it that way — it is why this project can be developed at all
without a 100 GB tree.

Directory-by-directory map: [`memory/MAP.md`](memory/MAP.md) (generated).

## 4. Rules the repository enforces on itself

Full list with the exact failure conditions: [`memory/INVARIANTS.md`](memory/INVARIANTS.md).
Short version — CI fails if a dependency has no provenance row or notice file, a
fingerprinting surface is undocumented, a code directory has no test, a UI value
breaks the style limits, a telemetry-shaped call appears, a catalog entry is
malformed, a speed claim has no number and unit, or the project memory is stale.

## 5. Where the work stands

Roadmap 1–46 landed across PRs #1–#12; 47+ is the next input from the project
owner. Current position, the last non-obvious findings and the open threads:
[`memory/STATE.md`](memory/STATE.md). Per-PR log: [`memory/HISTORY.md`](memory/HISTORY.md).
Decisions and their rationale: [`memory/DECISIONS.md`](memory/DECISIONS.md).

## 6. Working agreement for agents

1. Restore: this file → `memory/STATE.md`. Only then open code, using
   `memory/MAP.md` to pick the file instead of grepping the tree.
2. Before adding a dependency or reusing third-party code: `docs/LICENSING.md`
   and `docs/THIRD_PARTY.md` first, code second.
3. New component → new `*_test.cc` in the same directory, plus a `modules.json`
   entry if it is a new directory.
4. Every change ends with the memory update in
   [`memory/PROTOCOL.md`](memory/PROTOCOL.md) — in the same PR. CI checks it.
5. Do not state a performance number you did not measure; mark it `pending`.
