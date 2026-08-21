# Accessibility

**Roadmap item 60.** Chromium already ships the hard part: platform accessibility trees,
screen-reader bridges, focus management, caret browsing, zoom. An overlay that reimplemented any
of it would make things worse. What an overlay *can* break — and what forks routinely do break —
is its own surfaces: a privacy panel of unlabelled icons, a settings row no keyboard reaches, a
dialog that does not trap focus, an animation that ignores "reduce motion".

So Bedrock states its requirements as data (`src_overrides/bedrock/ui/accessibility.{h,cc}`), each
with the evidence that it is met, in the same shape as the feature registry of item 55. Nothing
here is claimed because it sounds good.

| Requirement | Bedrock's rule | Evidence |
| --- | --- | --- |
| Keyboard navigation | Every control is reachable without a pointer; every sidebar panel has a menu path *and* a shortcut | `ui/sidebar`, shortcut-uniqueness test |
| Screen readers | No surface without an accessible name and role; no custom painting that bypasses the tree | `ui/accessibility` (`Controls()`) |
| High contrast | High-contrast theme with a 7:1 floor; contrast validated for every theme, including user-made ones | `themes/theme_engine` (`ContrastRatio`, `kHighContrastFloor`) |
| Reduced motion | Motion is a setting with an off position; tokens carry a reduced-motion rule | `themes/theme_engine`, `branding/design-tokens.json`, `scripts/check_ui_style.py` |
| Scalable UI | Surfaces stay usable from 50% to 300%; hit targets never below 32 px | design tokens, `scripts/check_ui_style.py` |
| Visible focus | Every interactive element shows a focus ring; removing an outline without replacing it fails the build | `scripts/check_ui_style.py` |
| Accessible labels | Labels are words. Icon-only controls carry a name; privacy state is never colour alone | `ui/accessibility`, `docs/design/026-visual-language.md` |
| Dialog semantics | Dialogs declare a role, trap focus, close on Escape, and open focused on the safe choice | `ui/accessibility` (`ContractFor`), `settings/reset_controls` |

## The rule underneath all of it

**If a control can be operated, it can be operated from the keyboard and it has a name a screen
reader can read.** An icon is not a name.

`Accessibility::Controls()` is built from the surfaces themselves — the sidebar's panel table, the
reset actions, the advanced settings — so a new control cannot be added without a name. The test
fails the build, not a review comment.

## Destructive dialogs

A destructive dialog is an `alertdialog`, traps focus, closes on Escape, and **opens with focus on
the safe button**. Enter pressed by reflex must never erase a profile. Item 58's typed
confirmation is the second half of the same idea.

## Mockups are checked too

A mockup is the specification the UI gets built from, so `scripts/check_ui_style.py` holds the
mockups to the same rules: a `lang` attribute, a `:focus-visible` ring, a `prefers-reduced-motion`
block, real `<button>` elements instead of clickable `<div>`s, an `aria-label` on every icon-only
button, and `aria-hidden` on decorative glyphs.

## What is not claimed

Bedrock does not claim WCAG conformance. Conformance is a measured audit of a shipping build
against a specific level, and no Chromium build runs yet (item 55). What exists today is the set
of rules above, each enforced by a test or a gate; the audit comes with the first real build.
