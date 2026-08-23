# 045 — Settings, Privacy Center, extensions

Design items 21-25.

## Settings (21)

Left rail with nine sections — General, Privacy & Security, Search, Appearance,
Tabs, Downloads, Profiles, Extensions, Advanced — and content on the right. No
white cards, no search-box-as-a-substitute-for-structure, no accordion inside an
accordion.

The page is a *view over `ConfigSurface`*, never a second list of settings. That
is the point of `bedrock/settings/settings_page.h`: every key the browser has is
assigned to exactly one section, and a test asserts it. A setting nobody placed
lands in Advanced and stays visible rather than disappearing from the GUI while
still working from the config file — a setting the user cannot see is a setting
they cannot undo.

Each row carries its origin and whether policy has locked it, so the page says
*Managed by your organisation — set by policy* instead of showing a dead
control.

## Privacy settings (22)

Named options render as a segmented control, not a dropdown: three or four
choices with real names should be readable at a glance, and a dropdown hides two
of them. Free-form values get a plain field. The page never invents an option
that the model did not offer, which is also how a broken page fails safe.

## Privacy Center (23)

Large type, a single line of context, four glass panels, and a great deal of
empty space. `DashboardJson()` provides the figures and their formatting, so
"12,481" is written once, in C++.

Every number is a count of events the engine actually recorded on this device.
A counter at zero renders as zero, in muted type — a browser that has blocked
nothing on this profile must not be dressed up as if it had. The note under the
numbers says where they come from and that there is no server to send them to,
because a privacy dashboard is exactly the kind of screen that quietly becomes
an analytics upload in other products.

## Dashboard style (24)

Dashboard composition is used here and in the site panel, and nowhere else.
Bedrock is a browser: the tab strip, the address bar and the page are not a
place for metrics, sparklines or a "privacy score". A score would be the worst
of them — it compresses a set of real, checkable facts into a number the user
cannot verify and we cannot defend.

## Extensions (25)

Cards on dark surfaces, thin borders, muted text: name, version, what it can
read, whether it runs in private windows, its risk classification from
`ExtensionRegistry`, and an On/Off control. The permission line is the headline
rather than a rating, because what an extension can read is the only thing about
it that matters to privacy.

## Revision after review

The first pass was cramped: 13 px text, a rail of small labels floating at the
top, rows on a bare background. Fixed in the same PR series:

- The type scale moved up one step across the whole product (base 13 → 14 px,
  headings 24 → 28). It is a browser chrome, not a dense data grid.
- The rail has an icon and a 44 px row per section, so it reads as a list of
  places instead of small text in a corner.
- Rows live inside one card per section — the grouping Firefox gets right,
  without the white — with a 72 px minimum row height and the control vertically
  centred against the label.
- Each row now shows a short title with the full `--help` line underneath, and
  the line is dropped when it would just repeat the title. The GUI and the
  terminal still explain a setting with the same words, from the same table.

## Verification

- `settings_page_test.cc` — every `ConfigSurface` key belongs to exactly one
  section; unknown keys stay visible in Advanced; locked rows carry the policy
  origin; extension cards ride only with their own section.
- `privacy_center_test.cc` (new) — an empty log yields zeros, not invented
  numbers, and every row is a count of a recorded event.
- `check_ui_style.py` covers both new pages.

Not done here: binding the pages to the browser, which needs a Chromium build.
