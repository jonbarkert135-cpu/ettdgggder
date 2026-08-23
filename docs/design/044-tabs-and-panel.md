# 044 — Privacy indicator, tabs, sidebar, type

Design items 17-20, plus the typography change asked for in the same batch.

## Privacy indicator and panel (17)

The indicator in the address bar is a ring with a filled core, drawn from the
same tokens as everything else. Deliberately not a padlock and not a shield:
both are claims about safety. This control reports what the engine actually
applied on this page, which is a smaller and truer statement.

The panel behind it (`src_overrides/bedrock/ui/privacy_panel.html`) shows
trackers, ads, fingerprint protection, third-party cookies and connection, plus
an expandable list of the third parties blocked on this load. Every row comes
from `bedrock/ui/site_privacy_panel.h`, and the JSON keeps the distinction the
panel exists for:

- **count** rows are measurements from the privacy event log. They carry the
  copper accent, because they are the only numbers on the screen.
- **state** rows are the policy in force. No accent — a setting is not an
  achievement.
- rows that have not been measured render as *Not measured*, never as `0`.
  "We looked and found nothing" and "we have not looked" are different claims,
  and a browser that blurs them is training its user to trust a number that
  means nothing.

## Tabs (18, 19)

`TabModel` now has three layouts — horizontal, vertical, compact — and
`MetricsFor()` says what each one means in pixels, so the strip is not
re-invented per platform. All three render the same ordered list: a layout is a
rendering choice, never a second tab model, or a tab ends up existing in one
layout and missing in another with nobody able to reproduce it.

Compact is favicons only. The title moves to the tooltip and to tab search, so
nothing becomes unreachable — it stops being *visible*, which is what the user
asked for by choosing compact.

The active tab is one step brighter than the strip, with a hairline top
highlight and a soft border. Not a white slab: at forty tabs a bright active tab
is the only thing the eye can see, and the other thirty-nine become texture.

## Sidebar (20)

Off until asked for, with tabs, bookmarks, history, downloads, reading list and
workspaces. The existing invariant stands and is tested: every panel is also
reachable from the menu and from a keyboard shortcut, so "optional" does not
quietly become "optional unless you want your bookmarks".

Mockup: `docs/design/mockups/tab-layouts-dark.html` shows all three layouts and
the sidebar.

## Typography

The UI text font stays Inter — it is a working text face, and readability at
12-13 px is not where a browser should be expressive. Headings and the wordmark
now use **Poppins**, a geometric face with real character, which is where the
personality belongs.

Both are vendored as woff2 in `branding/fonts/` under the SIL Open Font License
and listed in `docs/THIRD_PARTY.md`. `gen_theme_css.py` emits the `@font-face`
rules and asserts the generated CSS contains no `https://` URL: a UI font pulled
from a CDN would be a request to a third party on every window the browser
opens.

## Provider marks in search

The engine selector shows a mark next to the provider name — a monogram in our
own style, not the provider's logo. Two reasons, in order: shipping another
company's brand asset in our chrome is a trademark problem (item 92), and a
vendored logo is stale the day they redesign. Site shortcuts use the site's own
favicon at runtime, with a monogram as the fallback.

## Verification

- `tab_model_test.cc` — the three layouts share one list; metrics differ and
  every layout keeps a hittable tab size.
- `site_privacy_panel_test.cc` — `PanelJson` keeps count/state/measured apart
  and escapes strings.
- `check_ui_style.py` and `check_branding.py` cover the new mockup and pages.

Not done here: binding these pages to the browser, which needs a Chromium build.
