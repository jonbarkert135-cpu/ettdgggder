# 043 — New tab, search field and browser chrome

Design items 11-16. What ships in this batch: the new tab page
(`src_overrides/bedrock/ui/new_tab.html` + `new_tab.js`), the state behind it
(`bedrock/ui/new_tab.h`), and the chrome composition in
`docs/design/mockups/browser-window.html`.

## The page (11, 12)

Wordmark, then one wide field, then a fast row of shortcuts, and a bookmark bar
pinned under the chrome. That layout is not novel and does not need to be: the
reason Chrome's new tab is quick is that the two things people actually use —
the field and the row under it — are within a few hundred pixels of each other
and nothing else competes for attention. Bedrock keeps the ergonomics and none
of the rest: no feed, no promoted tiles, no account nudges, no "customise"
button dropped over the corner.

Atmosphere is a single soft radial highlight over near-black and nothing else.
No imagery, no animation, no grain shader — the page opens hundreds of times a
day, and anything moving on it becomes noise by the second week.

## The field (13)

The only glass surface on the page (level 3 of the surface system): 52 px tall,
`--surface-glass` over the background, a 1 px border, a hairline inner
highlight, and a focus state that lifts the border and the shadow rather than
lighting up a ring. One field for search and address, because two would make the
user classify their own input.

Placeholder: *Search the web or enter address*.

## The engine selector (14)

A quiet control inside the field, showing the engine that will actually receive
the query, with a small menu listing what is installed (Google, DuckDuckGo).
It is built from the same tokens as everything else, so it reads as part of the
field rather than as a widget parked next to it. The page never resolves the
engine itself: the label and the list come from `NewTabJson()`.

## What the page is not allowed to decide

`bedrock/ui/new_tab.h` builds the shortcut row, and the rule it enforces is a
privacy one: shortcuts are derived from history, so in a private window the row
is empty and `historyHidden` is true — the page then says why instead of
looking broken. A new tab that quietly shows the normal profile's most-visited
sites is a leak to anyone looking at the screen. Bookmarks are not history; the
user placed them there, so the bookmark bar stays.

The rest of the row rules are also in C++ and tested: pinned first, no
duplicates, a cap, and a readable label derived from the host
("youtube", not a URL).

## Toolbar and address bar (15, 16)

Back, Forward, Reload, the address/search field, the privacy indicator inside
it, then Extensions, Downloads, Profile, Main menu. Nine controls, one row, no
overflow chevrons hiding a second row of buttons.

The privacy indicator lives *inside* the address bar because it describes the
page in that bar — how many requests were blocked on this site — and moving it
outside would make it look like a global toggle. It is the one place the copper
accent appears in the chrome.

The address bar is one step lighter than the toolbar, with a soft border. On
focus it gets slightly brighter, the border strengthens and a hairline inner
highlight appears; no glow, no colour change, no expanding animation.
Everything transitions within the 200 ms ceiling the style gate enforces.

## Verification

- `src_overrides/bedrock/ui/new_tab_test.cc` — labels, pinned ordering,
  deduplication, cap, the private-window rule, JSON escaping.
- `scripts/check_ui_style.py` covers both the mockups and the shipped pages:
  radius ≤ 16 px, blur ≤ 12 px, motion ≤ 200 ms, ≤ 2 gradients per file.
- The page was rendered in a browser against real `NewTabJson()` output.

Not done here: the WebUI host that binds `chrome.send('bedrockNewTab', …)` to
the browser, which needs a Chromium build.
