# ADR 0003 — Source layout: subsystem tree where it helps, Chromium's layout where it must

**Status:** accepted (2026-08-21) · roadmap item 47 · implemented in this change

## Context

Item 47 proposes a subsystem tree (`base/ chromium/ app/ ui/ privacy/{tracker_blocker,
fingerprinting,cookies,storage,network,permissions,security} search/ extensions/ profiles/
workspaces/ themes/ devtools/ downloads/ settings/ tor/ updater/ tests/ fuzz/ docs/
third_party/`) and says explicitly: do not copy it literally where Chromium's architecture
requires otherwise.

Bedrock is an overlay (ADR 0001). Files in `src_overrides/` are mirrored into a Chromium
checkout at the same relative path, so our layout is not a private matter — it decides where
code lands inside someone else's tree, whether `gn check` accepts the layering, and how badly a
Chromium rebase hurts.

## Decision

Adopt the proposed grouping for **Bedrock's own subsystems**, and refuse it wherever it would
duplicate or fight Chromium.

Everything Bedrock writes lives under one root: `src_overrides/bedrock/`. That root is now
organised by subsystem:

```
bedrock/
├── privacy/            the Privacy Engine, one tree
│   ├── core/           feature registry, PrivacyPolicy, Protection Controller, levels, telemetry policy
│   ├── fingerprinting/ anti-fingerprinting policy + deterministic derivation
│   ├── tracker_blocker/ filter engine, behavioral heuristic, the one blocking pipeline
│   ├── storage/        cookies and storage isolation (StorageKey)
│   ├── network/        DNS, HTTPS, WebRTC exposure
│   ├── security/       the security-baseline audit
│   └── stats/          the single privacy event log
├── ui/                 tabs, sidebar, per-site privacy panel
├── themes/             theme engine
├── settings/           Privacy Center, posture view, knowledge/
├── search/  omnibox/   search selection, omnibox input classification
├── extensions/         extension system + catalog/
├── profiles/  workspaces/  session/   profiles and New Identity · workspaces · browsing modes
├── history/  bookmarks/  passwords/  downloads/  devtools/  updater/
└── perf/  fuzz/         budgets · libFuzzer harnesses
```

### Where the proposal is deliberately not followed

| Proposed | What we did | Why |
| --- | --- | --- |
| `base/` | none | `//base` is Chromium's. A second base is how a fork starts. |
| `chromium/`, `app/` | `patches/` + `src_overrides/` mirroring Chromium's own paths | The Chromium tree is fetched, not vendored (ADR 0001). A directory pretending to hold it would be empty or a lie. |
| `privacy/cookies/` + `privacy/storage/` | one `privacy/storage/` | One `StorageKey` governs cookies and storage together (item 15). Two directories would invite two policies. |
| `privacy/permissions/` | Chromium's permission layer + `extensions/` disclosure | Permissions are a Chromium subsystem with its own UI and content settings. Bedrock changes disclosure and defaults, not the mechanism; a Bedrock `permissions/` would shadow `//components/permissions`. |
| `tor/` | `session/browsing_mode` | Tor is a **transport mode**, not a subsystem (invariant 9). A top-level `tor/` states the opposite of what the browser does. |
| `tests/` | `*_test.cc` beside the code | Chromium colocates tests, and so does `run_host_tests.sh`. A separate tree makes "which directory has no test" unanswerable — the check that catches subsystems added later. |
| `third_party/` | Chromium's `//third_party` + `THIRD_PARTY_NOTICES/` | We add no vendored copies; every dependency is a provenance row and a notice file. |
| `docs/` inside the source tree | repository-level `docs/` | Docs are not mirrored into the Chromium checkout. |

### Rules that follow

1. **Namespace names the subsystem, path names the placement.** `bedrock::net` may live in
   `privacy/network/` and `privacy/storage/`, exactly as Chromium's `net` namespace spans
   `//net/dns` and `//net/socket`. Renaming namespaces to match directories buys nothing and
   churns every call site.
2. **Include paths and header guards follow the path**, always: `bedrock/privacy/network/dns_settings.h`
   ⇒ `BEDROCK_PRIVACY_NETWORK_DNS_SETTINGS_H_`.
3. **One directory = one testable unit.** A directory with `.cc` files and no `*_test.cc` fails
   `scripts/check_security_testing.py`, at any depth.
4. **New directory ⇒ a line in `.ai/memory/modules.json`**, or `scripts/check_memory.py` fails.
5. Where Chromium already owns a concept (permissions, site isolation, autocomplete ranking),
   Bedrock configures and audits it instead of building a parallel one.

## Consequences

- This change is a pure move: 77 files relocated, includes and guards rewritten, no behaviour
  touched. All 34 test binaries and 4 fuzz harnesses build and pass unchanged, which is the
  evidence that it *was* a pure move.
- Path-referencing gates (`check_fp_docs`, `check_catalog`, `check_open_source`,
  `check_security_testing`) and the design docs were updated in the same change; the
  directory-has-a-test check now walks nested directories.
- Cost accepted: the tree is two levels deep in `privacy/`, so `grep -r bedrock/privacy` is
  broader than before. The map in `.ai/memory/MAP.md` is generated from the tree, so navigation
  does not depend on remembering the shape.
