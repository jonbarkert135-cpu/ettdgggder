# 026 — Visual language

**Roadmap item 26.** Status: tokens and a working mockup landed
(`branding/design-tokens.json`, `docs/design/mockups/browser-window.html`).

## Three influences, no imitation

- *Apple-level refinement* → restraint: one accent colour, few type sizes, generous spacing,
  motion you notice only when it is missing.
- *Windows-level usability* → nothing hidden behind a gesture; hit targets ≥ 32 px; every control
  reachable by keyboard; labels are words, not icons alone.
- *Linux-level transparency* → the UI shows the actual state of the engine (see
  [025](025-privacy-policy.md)): what was blocked, which scope decided a setting, what a mode
  cannot do.

No Apple, Microsoft, Google, Mozilla or Brave assets, layouts or names are copied — see
`docs/THIRD_PARTY.md`. Bedrock's own language:

**Graphite surfaces, one copper accent.** Neutral warm-grey chrome so page content is the only
saturated thing on screen, with a single copper accent (`#E08A4C` dark / `#B4622A` light) for
protection state and primary actions. Deliberately not blue (Chrome/Edge/Safari), not
orange-red (Brave, Firefox).

**Protection is a colour, not a badge.** When the accent appears in the toolbar, something is
being protected; the number next to it is a count, not decoration.

**4 px grid, 6/10/14 px radii, one shadow per elevation.** Tokens live in
`branding/design-tokens.json` and are the only place values are defined — a token set nobody can
hold in their head gets ignored, so it is short on purpose.

**Type.** Inter / SF Pro / Segoe UI Variable via a system stack, six sizes (11–24 px), three
weights. Tabular numerals wherever counts appear, so the shields panel does not jitter.

**Motion.** 90 ms for state, 160 ms for surfaces, one easing curve. `prefers-reduced-motion`
drops everything to 0 ms — a browser is a tool, not a demo.

**Writing is part of the design.** Every control carries one sentence of plain language, and the
honesty rules from items 19 and 22 apply to all of it: no "anonymous", no "100%", no promise the
engine cannot keep.

## Mockup

`docs/design/mockups/browser-window.html` is self-contained (no network, no fonts to fetch) and
renders the tab strip, omnibox with the protection chip, the mode chip, and the shields panel
whose rows are exactly the layers `PrivacyPolicy::Explain()` produces. It is the reference the
implementation is measured against, and it doubles as the visual regression target once the UI
is built.

## Surface system (dark by default)

Dark is the default theme, not a preference read off the OS. `tokens.css` is
generated from `branding/design-tokens.json` by `scripts/gen_theme_css.py`, so
there is exactly one palette and a stale stylesheet fails the build.
`[data-theme="system"]` is the only selector that follows
`prefers-color-scheme` — "System" is a choice made in setup (item 98).

Five levels, small steps between them, so depth reads as light rather than as
decoration:

| Level | Token | Where |
| --- | --- | --- |
| 0 background | `--surface` `#0B0C0D` | the window behind everything |
| 1 sunken | `--surface-sunken` `#08090A` | wells, hover on quiet rows |
| 2 raised | `--surface-raised` `#141517` | cards, panels, toolbars |
| 3 glass | `--surface-glass` `rgba(26,27,29,.72)` | rare: overlays and disclosure panels only |
| 4 focused | `--accent-quiet` + `--border-strong` | the selected row, nothing else |

Rules that keep it from becoming glass soup:

- **Glass is level 3 and nothing else.** Not buttons, not cards, not rows. Blur
  stays within the 12 px ceiling `check_ui_style.py` enforces.
- **One light source.** A single soft radial highlight, high and slightly left,
  at roughly 5% white. No second glow, no neon, no luminous borders. The
  gradient budget in the style gate is 2 per file, so this cannot creep.
- **Borders are barely there.** `--border` `#232527` at rest, `--border-strong`
  `#34373A` when something is active — low contrast on purpose, never a frame.
- **Radii are a scale, not a mood.** 6 px for small controls, 10 px for buttons
  and rows, 14 px for panels and modal surfaces. Pills are for chips only.
- **The accent stays scarce.** Copper marks protection state, focus and
  progress. The primary button is the brightest neutral instead, because a
  screen where every call to action shouts has no hierarchy left.
- **Air is a component.** 64 px page padding, 32 px between blocks, 24–28 px
  under headings. Secondary text is `#9B9EA2` — subdued, still above the 4.5:1
  floor from ACCESSIBILITY.md, never dimmed for atmosphere.
