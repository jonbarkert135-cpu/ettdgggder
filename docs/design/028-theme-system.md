# 028 — Theme system

**Roadmap item 28.** Status: landed and host-tested
(`src_overrides/bedrock/themes/theme_engine.{h,cc}`).

Modes: **Light · Dark · System · High Contrast · Custom**.

Properties, all live-adjustable: accent colour · background · tab shape · toolbar density ·
sidebar visibility · icon size · corner radius · transparency · blur · animations · font scale ·
spacing · compact mode · immersive mode.

## Three rules

**1. Values are clamped, never rejected in silence.** Every property has a `Range`; a value
outside it is clamped and the clamp is visible in the UI. This is also where item 27's limits
live, so the theme system physically cannot produce a 40 px radius.

**2. A theme cannot make the browser unusable.** `Validate()` returns warnings in plain language:

- accent vs background below 3:1 (AA for non-text) — or below **7:1** in High Contrast;
- translucency or blur switched on in High Contrast, which exists to remove exactly those;
- large font scale together with compact mode, because that combination clips labels.

`ContrastRatio()` is the real WCAG formula (sRGB linearisation, 0.05 offsets), tested against
black-on-white = 21:1 and mid-grey-on-white ≈ 4.5:1. A malformed colour returns 0 and is ignored
rather than applied as black.

**3. Switching mode keeps explicit choices.** Trying Light for a minute must not delete the icon
size the user set last week. The mode changes the baseline underneath; `Customized()` lists what
the user changed on top of it, and `Reset()` puts one property back.

Themes export to a small key/value file and import back. Unknown keys and malformed lines are
skipped — a theme from a newer version still mostly works, which is what makes sharing themes
realistic.
