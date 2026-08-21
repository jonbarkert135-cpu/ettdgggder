# ADR 0012 — Themes are tokens plus a manifest, evaluated by the browser, never code

**Status:** accepted (2026-08-21) · roadmap items 27, 28, 29 · owner's list: ADR-008
**Design notes:** `docs/design/026-visual-language.md`, `docs/design/028-theme-system.md`

## Context

Theming is the feature users ask for loudest and the one most likely to become an attack surface.
A theme format rich enough to be interesting is a program; a program shipped by a stranger and
executed inside the browser UI is a privileged extension by another name. Several browsers have
learned this in public.

The opposite failure is a "theme" that is four colours, which nobody wants.

## Decision

A theme is **data**: a manifest plus a set of design tokens, validated on load and applied by the
browser. Specifically:

* Tokens come from one vocabulary (`brand/design-tokens.json`) — surface, text, accent, border,
  state and elevation roles, not raw hex sprayed across widgets. `scripts/check_ui_style.py`
  enforces the vocabulary.
* A theme may set token values, supply images with declared dimensions, and choose from
  enumerated layout options. It may not ship script, CSS or fonts that are fetched at runtime.
* **Contrast is validated, not trusted.** A theme whose token combination falls below the
  accessibility contrast floor (item 60) is rejected at load with a message naming the pair. A
  pretty theme that makes the URL bar unreadable is a security problem, not a taste problem.
* Live customisation edits the same tokens, so what the user builds in the UI and what a shared
  theme file contains are the same object.

## Alternatives considered

* **Chromium's existing theme extensions.** Free, and it inherits an extension's permission
  surface plus a format we would not control. Rejected for the UI layer; unmodified Chromium
  theme support may still be honoured for imported themes.
* **CSS themes.** Expressive, unbounded, and a selector injection away from spoofing browser
  chrome. Rejected.

## Consequences

* Some visual ideas are simply not expressible. That is the price, and it is stated in the theme
  documentation rather than discovered by an author.
* The token vocabulary is a compatibility surface: renaming a token breaks published themes, so
  it is versioned in the manifest.
