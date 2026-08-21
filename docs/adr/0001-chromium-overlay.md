# ADR 0001 — Chromium base, overlay repository (not a fork)

**Status:** accepted, 2026-08-21
**Context:** master prompt sections 3–5.

## Decision

1. **Engine: Chromium**, pinned at a stable tag (`build/chromium.pin`, currently
   151.0.7922.173). Not Electron, not CEF, not a WebView wrapper, not a remote browser —
   Bedrock builds `chrome` from the real tree with our code compiled in.
2. **This repository is an overlay, not a fork of `chromium/src`.** It contains only
   `patches/`, `src_overrides/`, GN args, branding, docs and tooling. `build/sync.py` fetches
   Chromium via depot_tools and applies the overlay.

## Why overlay instead of forking the tree

A `chromium/src` fork is ~50 GB with history and must be rebased against a new upstream tag
every ~4 weeks; a merge conflict then spans the whole tree. Brave, ungoogled-chromium,
Vivaldi and Edge all run the overlay model for this reason. Consequences we accept:

- Every Chromium roll = re-apply `patches/` and fix the few that conflict, not a tree merge.
- New code goes into `src_overrides/` (new files never conflict) and is wired in with the
  smallest possible patch to an existing `BUILD.gn`.
- **Patch budget: keep the number of patches low.** A patch is a permanent maintenance tax.

## Autonomy is enforced at build time, not by policy

The "no vendor backend" rule (section 4) is implemented in `build/args/bedrock-release.gn`:
Google API keys empty, Safe Browsing off, reporting off, mDNS/remoting/hangouts off,
Chrome branding off. A network egress test in CI (planned, roadmap item) asserts a clean
profile makes **zero** connections to any `*.bedrock` or Google endpoint on startup.

Section 5 is respected: the browser still talks to whatever the *user* chooses (their search
engine, their DNS resolver, the sites they visit). We simply never insert our own infrastructure.

## Rejected alternatives

| Option | Why not |
|---|---|
| Electron / CEF | Explicitly excluded; not a browser, no site isolation control, no extension platform, no update/branding control. |
| Fork `chromium/src` | Rebase cost, repository size, and it hides our actual diff from reviewers and users. |
| Gecko / Firefox base | uBO-class extension ecosystem and the Chromium sandbox/site-isolation model are the requirement; Tor/Firefox contributions to this project are design-level (see `docs/THIRD_PARTY.md`). |
| Ladybird / Servo | Not production-ready for daily browsing in 2026. |

## Hardware reality (recorded so nobody is surprised)

A first Chromium build needs ~100 GB disk and, on 8–16 cores, 3–8 hours; incremental builds
are minutes. Bedrock CI therefore does **not** build Chromium on every PR — docs/lint/provenance
gates run per PR, full builds run on a dedicated builder (roadmap item).
