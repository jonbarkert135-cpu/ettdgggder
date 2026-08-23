# 047 — Semantic tokens, motion, grain, background

Design items 31-35.

## The vocabulary (31)

`tokens.css` now carries the semantic names the interface is written against,
generated from the same palette that already existed:

| Token | Resolves to |
| --- | --- |
| `--background-primary` / `--background-secondary` | window base, wells |
| `--surface-default` / `--surface-elevated` / `--surface-glass` | levels 2, 3, 4 |
| `--text-primary` / `--text-secondary` / `--text-muted` | three levels of emphasis |
| `--border-subtle` / `--border-default` / `--border-focus` | rest, active, focused |
| `--accent-primary` | protection state, focus, progress |
| `--shadow-soft` / `--shadow-elevated` | the two elevations in use |
| `--blur-subtle` / `--blur-strong` | 6 px, 12 px |
| `--radius-sm` / `--radius-md` / `--radius-lg` | 6, 10, 14 px |

The palette holds each value once; these names are how a page asks for it. That
is the difference between a theme system and a habit: a page that says
`--surface-default` keeps working when the shade changes, a page that says
`#141517` does not.

## The gate (32)

`scripts/check_tokens.py` reads every shipped page under
`src_overrides/bedrock/ui` and fails the build on any hex, `rgb()`, `rgba()` or
`hsl()` literal, and on any required token that `tokens.css` does not define.
It runs in `run_host_tests.sh`.

This is what turns item 32 from an intention into a property. All twelve pages
now pass it — the hairline highlights that used to be written as
`rgba(255,255,255,.06)` in six places are one token,
`--highlight-hairline`.

## Motion (33)

Durations live in the tokens and are checked by `check_ui_style.py`: 90 ms for
instant feedback, 160 ms for standard transitions, a 200 ms ceiling. Easing is
one token, `cubic-bezier(.2,.7,.3,1)` — quick to leave, gentle to arrive, which
is what "natural" means in practice.

What the codebase does not contain, by rule and by gate: zoom on open, spin,
bounce, parallax. Motion here exists to explain that a thing changed, and a
transition long enough to admire is a transition the user waits for a hundred
times a day.

## Grain and background composition (34, 35)

Both are one generated class, `.bedrock-canvas`, so every page composes the
background identically instead of each one inventing a gradient:

1. base colour — `--background-primary`
2. one soft radial light source, high and slightly left, at ~5% white
3. a grain layer at **3.5% opacity**, an inline SVG `feTurbulence` — no image
   file, no request, and nothing to load before the page paints

Both layers are `position:fixed` and `pointer-events:none`, so they cannot eat a
click and do not scroll with the content. Grain sits *under* the text, never
over it: the moment texture costs a reader legibility it stopped being a surface
quality and became an effect.

The atmospheric shape from item 35 is deliberately not implemented. At the
opacity where it would be tasteful it is invisible, and at the opacity where it
is visible it competes with the page. If you want it, it is one more layer on
this class — say the word.

## Verification

- `scripts/check_tokens.py` — 19 semantic tokens present, 12 pages free of raw
  colours.
- `scripts/gen_theme_css.py --selftest` — the canvas composition and the fonts
  are in the generated CSS, and no `https://` URL is.
- `scripts/check_ui_style.py` — radius, blur, duration and gradient budgets.
- Pages re-rendered in a browser afterwards; the grain is visible as texture and
  the text is not touched.
