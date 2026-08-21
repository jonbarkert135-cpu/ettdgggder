# ADR 0005 — Platform support and the abstraction that keeps it honest

- **Status:** accepted
- **Date:** 2026-08
- **Roadmap:** items 62, 63, 64

## Context

Bedrock targets Windows and Linux, with macOS as best effort (item 62), and owes each platform
native behaviour (items 63 and 64). Chromium already provides most of the machinery: the Windows
shell integration, the Wayland and X11 Ozone backends, per-monitor DPI, native file dialogs. What
an overlay adds is its own UI — and that is exactly where forks break platform behaviour, usually
by drawing their own titlebar or their own menus.

The failure that costs the most later is not a missing feature; it is `#ifdef` creep. Once
platform macros appear in feature code, the codebase becomes three codebases sharing a directory,
and the one nobody develops on rots silently.

## Decision

1. **Support tiers are stated and honest.** Windows and Linux are supported (built, tested,
   released). macOS is best effort and says why. A tier change requires changing a written reason.
2. **Platform macros live only in `src_overrides/bedrock/platform/`.** Feature code asks the
   platform layer for a capability; it never asks what platform it is on.
   `scripts/check_platform.py` fails the build otherwise.
3. **Integration points are a table, not folklore.** Every platform answers for all eleven points
   from items 63 and 64, each with an owner (Chromium-inherited or Bedrock-owned), the requirement
   and the failure mode it exists to prevent.
4. **No Linux desktop environment is assumed.** Capabilities are detected — XDG portals first,
   toolkit second, Bedrock default last. A requirement that names GNOME or KDE fails the gate.
5. **Both Linux display backends are first class.** Wayland is not experimental and X11 is not
   deprecated, because both describe large parts of the user base today.
6. **Several package formats, each documenting its sandbox implications**, with a plain tarball
   always available as the format every distribution can use.

## Consequences

- Adding a platform means filling in a table, not scattering branches; a gap fails a test.
- Some Bedrock-owned surfaces (titlebar, menus, notifications) need per-platform implementations.
  Those live behind the platform layer's interfaces, so shared code stays single-copy.
- macOS work is bounded: keep it buildable and keep the abstraction clean. No release promises
  until someone tests it.
- The Wayland-first choices (portals for file chooser, colour scheme and notifications) also
  improve the Flatpak build, since the same interfaces are what a sandbox allows.

## Alternatives rejected

- **Windows-only first.** Faster, but a Linux port added later inherits every assumption made in
  the meantime — the abstraction has to exist while there is something to abstract.
- **A toolkit of our own.** Weeks of work to reimplement what Chromium already does across three
  window systems, with worse accessibility (item 60) at the end of it.
- **Assuming one Linux desktop.** Cheaper to write and the reason many applications feel foreign
  on most desktops.
