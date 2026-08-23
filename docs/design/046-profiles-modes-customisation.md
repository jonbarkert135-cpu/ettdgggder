# 046 — Profiles, window modes, customisation

Design items 26-30.

## Profile selector (26)

Small: the active profile with one line under it, the other profiles as switch
targets, the window mode when there is one, and two actions — New profile,
Manage profiles. No avatar gallery, no account marketing.

The line under the name is the interesting one. Bedrock has no sync service, so
it reads *Not synced — this profile stays on this device* instead of leaving a
gap where other browsers put a sign-in prompt. A temporary profile says
something stronger: everything here is discarded on close. If self-hosted sync
is ever added (item 95), its real state goes on that line — never a badge that
implies a server we do not run.

## Private and Tor windows (27, 28)

Same design system, different state. `ThemeCss()` shifts the two surface
colours a step darker (30% for private, 45% for Tor) and leaves text and accent
alone, so a private window is recognisable at a glance while still being the
user's own theme. No purple neon, no hooded figure, no borrowed Tor branding —
we are not permitted to use their marks and would not want to: a window that
looks like a toy gets trusted for the wrong reasons.

Each mode carries one sentence, from `IdentityFor()`, and each sentence states
the limit as well as the benefit:

- **Private** — keeps no history, cookies or cache after you close it. *Your
  network and the sites you sign in to still see you.*
- **Tor** — routes through Tor, which hides your address from the sites you
  visit. *It is slower, and some sites refuse it.*

A test asserts both halves are present. A mode badge that only lists benefits is
how users end up believing private browsing hides them from their employer.

## The default dark theme (29)

Covered by the token system from PR #30 and the type work in PR #32: near-black
graphite, one light source, one accent used only for state, Poppins headings
over Inter text, generous spacing. The measurable parts are enforced by
`check_ui_style.py`; the rest is visible in the screenshots attached to those
PRs.

## Customisation (30)

`ThemeEngine` already carried accent, background, tab shape, density, sidebar,
icon size, radius, transparency, blur, animations, font scale and spacing. This
batch adds the rest of the list: **surface colour, text colour, grain, shadow
strength, glow, transition duration**.

`themes/theme_css.h` is the new bridge: it turns the engine's state plus the
window mode into a `:root` override block over `tokens.css`. Two properties of
that design matter:

- **Customisation cannot break the browser.** Values pass through the engine,
  which clamps them, so a 400 px radius, a 90 px blur or a 5 s transition never
  reach the stylesheet — the test asserts exactly that. There is no path from a
  settings page to arbitrary CSS.
- **Motion off beats motion fast.** With animations disabled the duration is
  zero regardless of the transition setting, because nobody switches animations
  off in order to get a quicker one.

## Verification

- `theme_css_test.cc` — clamping, the animations-off rule, the user's accent
  surviving a mode change, private ≠ Tor, and both halves of each mode
  sentence.
- `profile_menu_test.cc` — the active profile is not repeated in the switch
  list, sync state is stated rather than implied, no sign-in prompt exists.
- `theme_engine_test.cc` — unchanged and still passing with the new properties.

Not done here: applying the generated block to a live window, which needs a
Chromium build.
