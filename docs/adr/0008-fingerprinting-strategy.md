# ADR 0008 — Fingerprinting: deterministic per-site perturbation, in levels

**Status:** accepted (2026-08-21) · roadmap items 9, 10 · owner's list: ADR-003
**Design note:** `docs/design/010-anti-fingerprinting.md` · surfaces: `docs/privacy/fingerprinting/`

## Context

Bedrock's users are people who want a normal browser that does not sell them, not people evading
a state adversary. Tor mode exists separately for the second case (ADR 0010). The strategy has to
protect the first group without making the browser unusable for them.

## Alternatives considered

Two defensible strategies exist, and they are opposites.

**Uniformity** (Tor Browser): make every user look identical. It works, and it costs the things
that make a browser usable outside a threat model that justifies them — a fixed window size, one
locale, no GPU, no plugins.

**Randomisation** (Brave's farbling): make every user look different, per site, per session. The
cross-site identifier disappears without the user noticing anything. Its weakness is that noise
which changes *within* a site is both detectable and breaks pages.

## Decision

**Deterministic per-site, per-session perturbation, arranged in four levels.**

The perturbation value is derived from `(site, session seed)` with a fixed function, so a page
reading canvas twice gets the same answer, a page reading it after a reload within the session
gets the same answer, and two different sites get different answers. That single property is
what makes the difference between "protected" and "broken".

* **Level 0** — off, for a site the user has exempted.
* **Level 1 (Balanced, the default)** — normalise what costs nothing (language list, timezone,
  hardware counts, client hints), perturb canvas and WebGL readback, withhold the debug renderer
  string.
* **Level 2 (Strict)** — add screen-metric letterboxing, font-list restriction, timer coarsening.
* **Level 3** — the uniformity posture, used by Tor mode, where the cost is accepted by the user
  choosing that mode.

## What this does not claim

Perturbation is detectable: an unusual answer is itself a signal, and a site determined to know
it is being protected will find out. The goal is not invisibility, it is destroying the
*cross-site linkability* of the value. `docs/privacy/FEATURES.md` states that limit per surface,
and `tests/privacy/` measures the result against a recorded stock-Chromium baseline.

## Consequences

* The derivation function is security-relevant: a bug in either direction either breaks sites or
  creates a new stable identifier. It is one function, tested, and it is the reason canvas
  protection carries a complexity score of 3.
* Per-site exceptions are a first-class feature, not an escape hatch — image editors and colour
  pickers legitimately need real readback.
