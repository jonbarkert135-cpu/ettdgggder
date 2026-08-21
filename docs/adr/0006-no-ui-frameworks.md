# ADR 0006 — No JavaScript UI framework

**Status:** accepted, 2026-08-21
**Roadmap item:** 78
**Gate:** `scripts/check_frameworks.py`

## Context

Bedrock's interface has two kinds of surface:

* **native** — window frame, tab strip, omnibox, menus, dialogs. Chromium draws these with
  Views, and they must look and behave like the rest of the desktop.
* **WebUI** — Settings, the Privacy Center, the extension catalog, DevTools panels. Chromium
  renders these as privileged pages inside its own renderer.

For the second kind the question comes up every time: which framework? The honest answer is
that the question is borrowed from web development, where you cannot assume the runtime. Here
the runtime *is* the product. The page is rendered by the very engine being shipped, of a
version known at build time.

## Decision

WebUI surfaces are written with the platform: **custom elements**, shadow DOM, `<template>`,
plain CSS with design tokens, `fetch` and the WebUI mojo bindings Chromium already provides.
No React, Vue, Angular, Svelte, jQuery, Bootstrap or Tailwind; no webpack, Vite, Rollup or
Babel; no `npm install` between a checkout and a build.

## Why

1. **A framework is shipped code.** Every kilobyte is audited, updated on someone else's
   release schedule, and present in the address space of a privileged page. The settings page
   of a privacy browser is the worst place to run a large dependency whose supply chain is not
   ours (see item 77 and `docs/DEPENDENCIES.md`).
2. **Chromium already has the abstractions.** Components, templating, reactivity through
   observers, routing between subpages — the platform does all of it, and the platform version
   ships anyway.
3. **A browser is not a website packaged as an application.** That is the phrase item 78 uses,
   and it names a real failure mode: a UI that reinvents scrolling, focus, keyboard handling
   and accessibility in JavaScript, badly, on top of a browser that had all four correct.
   Item 60's accessibility conformance depends on native semantics, not on re-implemented ones.
4. **Startup and memory are user-visible.** A framework bundle parsed on first paint of the
   settings page competes with the page the user actually wants (item 46's budgets).
5. **Build reproducibility.** No npm registry means one less non-reproducible input to the
   release (items 41 and 70).

## What this rules in

Small, single-purpose helper modules written here and reviewed here — a table sorter, a token
formatter — are fine. The rule is about *frameworks and bundlers*, not about ever writing a
function twice.

## Consequences

* Contributors write more explicit DOM code. That is accepted: it is less code overall than a
  framework plus its glue, and it is code we can read at 3 a.m. during a security fix.
* If a WebUI surface ever genuinely outgrows the platform, this ADR gets superseded with the
  measurement that proved it — not with a preference.

## Alternatives considered

* **Lit** — small and close to the platform, but still a dependency to pin, audit and update
  for a benefit (`html` templating) that `<template>` plus custom elements already provides.
* **Preact** — smaller than React, same argument; the component model is not the bottleneck.
* **A design-system CSS framework** — rejected in favour of `brand/design-tokens.json`, which
  `check_ui_style.py` already enforces.
