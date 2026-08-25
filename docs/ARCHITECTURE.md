# Architecture

**Roadmap item 72.** How Bedrock is put together, and — more useful — where a change belongs.
Detail per subsystem lives in the generated map, [`.ai/memory/MAP.md`](../.ai/memory/MAP.md);
this document is the shape.

## The one structural decision

Bedrock is an **overlay on Chromium**, not a fork ([ADR 0001](adr/0001-chromium-overlay.md)).
This repository contains no copy of Chromium. It contains new source files, a small set of patches,
build arguments, branding, documentation and tooling; `build/sync.py` fetches the pinned Chromium
tree and lays the overlay over it.

Everything else follows from that:

- Chromium's own architecture is authoritative. Where a subsystem must live in Chromium's tree, it
  lives there — the layout below is the parts that are *ours*
  ([ADR 0003](adr/0003-source-layout.md)).
- A new file is always preferable to a patch. New files do not conflict on an upstream roll; a
  patch conflicts eventually, and every conflict is a chance to reintroduce a fixed bug
  ([`PATCHES.md`](PATCHES.md)).
- A security roll must stay a rebuild, not a research project. That is a design constraint, not
  release hygiene ([`UPSTREAM_SYNC.md`](UPSTREAM_SYNC.md), item 69).

## Process model

Chromium's, unchanged and unweakened: browser process, one renderer per site instance, GPU,
network and utility processes, all sandboxed, with site isolation on. Bedrock adds no process type
and removes no isolation. `privacy/security` fails the build if a switch that weakens the sandbox,
site isolation or the security baseline appears in the release GN args — the cheapest way to make a
privacy browser insecure is to disable a Chromium protection for a debugging session and forget.

Where Bedrock code runs:

| Code | Process | Why there |
| --- | --- | --- |
| privacy engine, policy resolution, stats | browser | one source of truth; a renderer must not decide its own protections |
| filter engine (matching) | browser (network service path) | requests are already there; no per-renderer copy of the rule set |
| fingerprinting value derivation | renderer, values supplied per-origin from the browser | the value must be stable per origin and unforgeable by the page |
| UI (tabs, panels, settings, privacy centre) | browser, TypeScript/HTML for surfaces, C++ for chrome | item 48 |
| storage keying, cookie decisions | browser | one `StorageKey` decides for every backend |

## Layout

```
src_overrides/bedrock/
├── privacy/          core (policy, features, honesty), tracker_blocker, fingerprinting,
│                     storage, network, security, stats
├── ui/               tabs, panels, accessibility, l10n (string catalog)
├── settings/         privacy centre, configuration surface, advanced, reset, import/export
├── session/          normal / private / Tor transport, circuit isolation
├── profiles/         profiles that share nothing, New Identity
├── workspaces/       named tab sets inside a profile
├── search/ omnibox/  engine selection per context, input classification
├── history/ bookmarks/ passwords/ downloads/
├── extensions/       registry, capability disclosure, curated catalog
├── themes/ devtools/ perf/ platform/ updater/ fuzz/
build/                chromium.pin, sync.py, GN args, SBOM, dependency hashes
patches/              upstream diffs (currently empty by design)
docs/                 this tree
scripts/              gates and tooling — every one of them runs in CI
.ai/                  project memory (read .ai/MEMORY.md first)
```

Every directory under `src_overrides/bedrock/` carries a summary in
[`.ai/memory/modules.json`](../.ai/memory/modules.json) and at least one colocated `*_test.cc`;
`check_memory.py` fails when a new one appears without either.

## How a request flows

The path that defines the product, from typing to pixels:

1. **Omnibox** classifies the input (search vs URL) without contacting anything.
2. **Search / session** decide the engine and the transport for this context — a private window and
   a Tor session do not inherit the normal default.
3. **Network privacy** resolves DNS through the configured resolver (fail-closed in strict mode),
   upgrades to HTTPS, applies referrer and Client-Hints policy.
4. **Tracker blocker** matches the request against the rule set (rarest-token index) and returns
   allow / block / modify.
5. **Storage** keys every cookie and storage access by `StorageKey`, so third-party state is
   partitioned rather than merely "cleared later".
6. **Renderer** gets fingerprinting values derived per origin — normalised first, never randomised
   for its own sake.
7. **Stats** records exactly the events that happened, and only real ones (item 55). The panel, the
   shield and DevTools all read that one log; there is no second counter for the UI.

## The privacy architecture, in one picture

Trust boundaries first: the question that decides every design argument here is *which side of a
boundary a decision is made on*. A page may never decide its own protection, and the renderer that
runs its script is not trusted to enforce one.

```
        ── untrusted ──────────────┊─── trusted (browser process) ──────────────────┊── outside ──
                                   ┊                                                 ┊
   ┌───────────────┐               ┊   ┌──────────────────────────────────────┐      ┊
   │ page / script │               ┊   │ privacy engine  privacy/core         │      ┊
   │  (site code)  │               ┊   │  · feature registry (30)             │      ┊
   └───────┬───────┘               ┊   │  · per-site policy resolution        │      ┊
           │ DOM / JS reads        ┊   │  · protection levels, honest flags   │      ┊
   ┌───────▼───────┐  values per   ┊   └───────┬─────────────┬────────────────┘      ┊
   │   renderer    │◄──origin──────┊───────────┘             │ decides for ↓         ┊
   │ (sandboxed,   │  (never a     ┊                         │                       ┊
   │  per site)    │   secret)     ┊   ┌─────────────────────▼───────────────┐       ┊
   └───────┬───────┘               ┊   │ request path                        │       ┊
           │ request               ┊   │  1 omnibox   classify, contact none │       ┊
           └───────────────────────┊──►│  2 session   engine + transport     │       ┊
                                   ┊   │  3 network   DNS, HTTPS, referrer   │──────►┊ resolver,
                                   ┊   │  4 blocking  lists + heuristic      │       ┊ site, Tor
                                   ┊   │  5 storage   one StorageKey         │       ┊
                                   ┊   └─────────────────┬───────────────────┘       ┊
                                   ┊                     │ only what happened        ┊
                                   ┊   ┌─────────────────▼───────────────────┐       ┊
                                   ┊   │ stats → shield, panel, Privacy Centre│      ┊
                                   ┊   └──────────────────────────────────────┘      ┊
                                   ┊                                                 ┊
                                   ┊   local disk: profiles, encrypted passwords     ┊
                                   ┊   (no cloud config, no sync, no telemetry)      ┊
```

Read it as five rules, each with the gate or test that keeps it true:

| Rule | Why | Kept true by |
| --- | --- | --- |
| Policy is resolved in the browser process, never in a renderer | a compromised renderer must not be able to grant itself an exemption | `privacy/core` owns every decision; ADR 0007 |
| The renderer receives *values*, never the session secret | a leaked secret would make every surface forgeable and cross-site linkable | keyed derivation per (secret, eTLD+1, surface), F4 fix, `crypto/hash` |
| A request is decided before it leaves, not cleaned up afterwards | a blocked request that was already sent is not blocked | `BlockingPipeline` order; `docs/design/013` |
| Everything the user is shown comes from the event log | a counter invented for the UI is how a browser starts lying about itself | `privacy/stats`, item 55, `check_no_fake_features.py` |
| Nothing crosses the right-hand boundary that the user did not ask for | item 94: no hidden cloud | `remote_features.cc` + `check_remote_features.py`, `check_no_telemetry.py` |

## Languages

C++ for engine integration and anything security- or performance-critical, Rust for new isolated
memory-unsafe-input parsers, TypeScript/HTML/CSS for UI surfaces, Python for build and gate tooling.
No Electron anywhere ([ADR 0004](adr/0004-languages.md); `check_languages.py` enforces it). No Rust
module exists yet — the first candidate is the filter-list parser, and it will arrive as a module
with its own boundary, not as a rewrite.

## Where a change belongs

| Change | Goes |
| --- | --- |
| new privacy behaviour | `privacy/<area>/`, registered as a feature in `privacy/core`, with an honest UI-renderable flag |
| new user-visible string | `ui/l10n/string_catalog.cc`, all four locales, no hardcoded text |
| new setting | `settings/config_surface.cc` — GUI + config file + policy + CLI in one place ([`CONFIGURATION.md`](CONFIGURATION.md)) |
| change to Chromium behaviour | `src_overrides/` first; a patch only if an existing upstream file must change |
| new third-party component | inventory row + notice file **before** the code ([`LICENSING.md`](LICENSING.md)) |
| new invariant | `.ai/memory/INVARIANTS.md` plus the gate that checks it |

## What is not built yet

The overlay compiles inside a real Chromium tree (builds of 2026-08-22 and 2026-08-23) and exactly
**one** feature is `kEnforced` — `webrtc_policy`, verified in the browser. Every other privacy
feature is `kDesigned` or `kImplemented`: real, host-tested logic that no Chromium call site invokes
yet. The status of each one is data, not prose — see [`ACCEPTANCE.md`](ACCEPTANCE.md) for the 31
acceptance criteria and [`../build/ENFORCEMENT.md`](../build/ENFORCEMENT.md) for what a build has
actually proven. Turning `kImplemented` into `kEnforced` is the whole of the remaining work, and it
needs a full build per change, which is why phases 3–15 landed before phase 1
([`PHASES.md`](PHASES.md) is blunt about that trade).
