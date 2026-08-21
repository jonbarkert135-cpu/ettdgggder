# 007 — Address bar / omnibox

**Roadmap item 7.** Status: classification logic landed and host-tested
(`src_overrides/bedrock/omnibox/input_parser.{h,cc}`, `input_parser_test.cc`).

## What Bedrock adds, and what it does not

Chromium's `AutocompleteController` already produces bookmark, history, open-tab, clipboard and
search-suggestion matches, ranked and deduplicated, with years of edge-case handling. Bedrock
**keeps all of it**. Reimplementing that ranking would be a second, worse ranker to maintain.

Bedrock adds only the two things Chromium has no concept of:

1. **Bangs** — `!g linux kernel`, `linux kernel !g` → the matching provider.
2. **Browser commands** — `>clear history`, `>shields off`, `>new private window`.

Everything else the parser labels `kSearch` flows into the normal provider pipeline, so
bookmarks, history and tab matches still appear for those inputs.

## Classification order

`ParseOmniboxInput()` decides in this order:

| # | Rule | Example | Result |
|---|---|---|---|
| 1 | Leading `>` | `>clear history` | command (`text` = "clear history") |
| 2 | Known bang at start or end | `!g linux`, `linux !g` | search via that provider |
| 3 | Looks like a URL | `example.com`, `localhost:8080`, `bedrock://settings`, `/etc/hosts` | navigate |
| 4 | Otherwise | `best laptops` | search via the omnibox-context engine |

Deliberate decisions, each covered by a test:

- **An explicit bang beats URL detection.** `!g example.com` searches for the string; the user
  who typed a bang meant it.
- **An unknown bang is ordinary text.** `!unknown thing` and `!!!` are searched verbatim —
  silently dropping a token the user typed is worse than an unhelpful search.
- **A bang alone** (`!br`) selects the provider with an empty query, which is how a
  "search this site next" flow starts.
- **`host:port` is not a scheme.** `localhost:8080` navigates; `mailto:a@b` also navigates.
- **Whitespace means query.** `example. com` is a search, not a typo-corrected navigation.
- The `>` escape exists so a command can never be shadowed by a site or a bookmark of the same
  name — the ambiguity Chromium's own "@" scopes struggle with.

## Command surface (initial)

`>` opens a command list filtered as you type. Commands are registered by module, each with an
id and a title string, and every command must be reachable from the UI as well — the omnibox is
a shortcut, never the only path. Initial set: new tab / new private window, clear history,
clear cookies for this site, shields on/off for this site, open settings, open downloads,
open the privacy log, reload without content blocking.

## Privacy notes

- With suggestions off (the default) the omnibox makes **zero** network requests while typing.
- In private windows suggestions stay off regardless of the pref (see 006).
- No omnibox input is ever logged off-device; there is no metrics endpoint to log to.
- Prefetch/preconnect on typing is off by default: it leaks the guess before the user commits.

## Follow-ups

- Inline autocomplete for high-confidence history matches (Chromium provides it; needs tuning
  once shields interact with prefetch).
- Custom user keywords beyond provider bangs (`gh` → GitHub repo search) — same parser, the
  keyword table simply grows.
