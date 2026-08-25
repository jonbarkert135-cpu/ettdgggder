# 050 — Letterboxing

Top of the research queue in `.ai/memory/STATE.md`, arrived at twice
independently: [`docs/research/FIREFOX.md`](../research/FIREFOX.md) called it
"the one concrete piece we do not have", and
[`docs/research/TOR_BROWSER.md`](../research/TOR_BROWSER.md) marked it *adopt* —
a UI-level constraint, not a Gecko internal, so it is implementable on Chromium.

## The hole it closes

Window geometry is the fingerprinting surface nobody has to ask for. A page
reads `innerWidth`, `screen.availHeight` or one `getBoundingClientRect()` and
learns a value that is high-entropy (a browser window is sized by a human, on a
specific display, with a specific taskbar and specific chrome) and stable for
the whole session. No permission, no API call worth blocking, no way to notice.

`fingerprint_policy` already decided *which* sizes a page may see:
`QuantizeWindowSize()` rounds down to a step (50 / 100 / 200 px by level). What
was missing was the other half — the page still **rendered** into the real
window. Reporting 1300×700 while laying out 1366×768 is not a protection: two
lines of JavaScript compare the reported size against a measured element and
recover the original number, and the browser has traded a leak for a lie.

`privacy/fingerprinting/letterboxing` closes that: the page renders into the
quantised box, centred, with the leftover pixels painted as a neutral margin.
Reported size and rendered size are the same number, because they are the same
box.

## Where it sits

```
real window size ──► QuantizeWindowSize()  (steps, per level: the policy)
                        │
                        ▼
                  ComputeLetterbox()       (content box + margins: the geometry)
                        │
        ┌───────────────┼────────────────────────────┐
        ▼               ▼                            ▼
   render target   innerWidth/Height     ReportedScreenSize() → screen.*
```

One geometry source for the whole `screen` surface, and the step table stays
where it already was — no second copy (`MEMORY.md` non-negotiable 4).

## Five decisions

Each is a decision to do *less* than the obvious thing, and each has a named
assertion in `letterboxing_test.cc`.

**1. No fullscreen exception.** The tempting carve-out — "don't letterbox
fullscreen video" — hands back exactly the entropy the surface exists to remove:
`requestFullscreen()` is the cheapest way to ask a window how large the display
really is. `ComputeLetterbox()` therefore has no fullscreen parameter at all, so
the exception cannot be added by accident in a call site.

**2. Margins are centred, and the odd pixel always goes right/bottom.** A
randomised or varying split would be a per-window signal — the "your noise is
your fingerprint" failure that rule 2 of
[ADR 0008](../adr/0008-fingerprinting-strategy.md) forbids. Same window, same
box, every call.

**3. No site input.** The function takes `(window, level)` and nothing else, so
two tabs in one window cannot be distinguished by their viewport and an iframe
cannot re-sample the surface under another origin.

**4. Two guards, so the mechanism cannot eat the window.** An absolute floor
(200×100) and a proportional one (the page keeps ≥ 60% of the pixels). A 320×240
window at level 3 would be squeezed to 200×200 — half the pixels for entropy
that barely exists at that size, because almost no window is that small. Both
guards leave the window untouched, `Letterbox::active()` is false, and the
privacy panel says the protection is not applied rather than claiming one that
is not.

**5. A resize only reaches the page at a bucket boundary.** `ViewportChanges()`
is the relayout test: dragging a window from 1366×768 to 1399×799 changes
nothing observable. Without it, a slow drag streams hundreds of distinct sizes
to the page and the quantisation buys nothing.

## What it costs the user

Visible bars at the higher levels — the honest price of the strongest
anti-linking measure available, and the reason this belongs to the levels that
already state a compatibility cost:

| Level | Step | 1366×768 becomes | Pixels given up |
| --- | --- | --- | --- |
| 0 Compatibility | — | 1366×768 | none |
| 1 Balanced | 50 px | 1350×750 | ~3% |
| 2 Strict | 100 px | 1300×700 | ~13% |
| 3 Maximum | 200 px | 1200×600 | ~31% |

Responsive layouts behave normally: a quantised viewport is a perfectly valid
viewport, just not the one the window has.

## Status

Host-tested logic, no entry point yet. Applying the box needs the browser
shell — the content view's size constraint, the margin paint and the
`screen.*`/`innerWidth` shims all live in the Chromium build (phase 3), and
`docs/PHASES.md` keeps that honest. No new feature id: this is the geometry of
`screen_metrics`, whose disclosure text was updated in the same change to say
that small windows are not letterboxed at all.
