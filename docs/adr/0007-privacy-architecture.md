# ADR 0007 — Privacy is one engine with a registry, not a bag of switches

**Status:** accepted (2026-08-21) · roadmap items 8, 11, 55 · owner's list: ADR-002
**Design notes:** `docs/design/008-privacy-engine.md`, `docs/design/011-protection-controller.md`

## Context

Every privacy browser accumulates protections one at a time: a cookie setting here, a
fingerprinting toggle there, a per-site exception list bolted on later. The result is familiar —
two features that disagree about whether a request is third-party, a settings page offering a
control for something the browser does not do, and no single place to ask "what is protecting
this page right now?".

Bedrock has thirty protections. Without a structure, thirty is exactly the number at which the
above becomes unavoidable.

## Decision

One **Privacy Engine** with a **feature registry** as the single source of truth. Every
protection is a row: id, module, title and explanation string ids, standard and strict defaults,
whether it breaks sites, and its `Status`.

Three consequences follow from the registry being data rather than code:

1. **The UI is generated from it.** `UiRenderableFeatures()` returns only features with
   `Status::kEnforced`, so the settings page physically cannot offer a switch for a protection
   the running browser does not perform (item 55). It returns nothing today, and that is the
   honest answer while no Chromium build exists.
2. **Resolution is one function.** The Protection Controller answers site → domain → global →
   built-in for every control, with no per-feature special cases; a special case here is a
   security hole someone reasons about incorrectly later.
3. **Disclosure is attached to the row.** Item 82's four statements and item 85's five scores
   live in a parallel table keyed by the same feature id, and a host test fails if a feature
   exists without them.

## Alternatives considered

* **Per-module settings, no registry.** What most browsers have. Rejected: it makes "what is on
  right now" unanswerable without reading five subsystems.
* **A rules engine with priorities.** Rejected: priority numbers are how a resolution order
  becomes folklore. Scope specificity is the only ordering, and it is total.

## Consequences

* Adding a protection means adding a registry row, a disclosure row and a design note. That
  friction is deliberate; it is also why the count of protections is a real number.
* Nothing may claim `kEnforced` until it is verified in a Chromium build and recorded in
  `build/ENFORCEMENT.md`.
