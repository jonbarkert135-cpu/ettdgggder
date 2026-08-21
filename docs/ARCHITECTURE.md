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

Nothing in this repository has been compiled against a real Chromium tree: the host tests build the
overlay's logic standalone. Every privacy feature is therefore `kDesigned` or `kImplemented`, never
`kEnforced` — that status changes only after a real build verifies the behaviour
([`.ai/memory/STATE.md`](../.ai/memory/STATE.md)).
