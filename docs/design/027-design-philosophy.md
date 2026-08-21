# 027 — Design philosophy

**Roadmap item 27.** Status: limits encoded in `ThemeEngine` and enforced by
`scripts/check_ui_style.py` (wired into `run_host_tests.sh`).

> The interface must feel like a real desktop application, not a web dashboard.

What that means concretely, and how each part is kept true:

| Rule | How it is enforced |
|---|---|
| No giant rounded cards | corner radius ≤ **16 px** (`ThemeEngine::kMaxCornerRadius`, tokens, mockup scan) |
| No glass soup | blur ≤ **12 px**, and translucency ≥ 0.80 opacity |
| No heavy animation | every duration ≤ **200 ms**, one easing curve, `prefers-reduced-motion` → 0 ms |
| No decorative gradients | ≤ 2 gradients per surface file; none on interactive chrome |
| Accessible | contrast floors are validated (AA for controls, AAA in high contrast); hit targets ≥ 32 px |
| Keyboard-friendly | every sidebar panel and every mode has a shortcut, and shortcut uniqueness is a test |
| Fast | changes are repaints or relayouts, never restarts (item 29) |
| Coherent | one token file; the shields panel text comes from `PrivacyPolicy::Explain()` |

The gate is the point. "Clean and minimal" as prose survives exactly one deadline; as
`check_ui_style.py` it survives the project. A radius of 24 px or a 400 ms transition fails CI
the same way a broken test does.

### Desktop, not dashboard

- Native window controls, native menus, native context menus — no reinvented widgets.
- Density is a setting, not a fashion: compact / default / comfortable, all shipped.
- Text is selectable, lists are keyboard-navigable, and every control has a real focus ring.
- No decorative iconography without a label anywhere a decision is made.
- Chrome stays out of the way: the page is the only saturated thing on screen (item 26).
