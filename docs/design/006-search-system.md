# 006 — Search engine system

**Roadmap item 6.** Status: designed, code landed for the selection logic and provider data.

## Model

A provider is **data**, never code. `src_overrides/bedrock/search/bedrock_search_engines.json`
holds the built-in set (DuckDuckGo, Google — both required — plus Brave Search, Startpage,
Bing, Mojeek, Ecosia, Wikipedia). User-added providers land in the profile with the identical
schema, so "built-in" and "custom" differ only in origin. Nothing in the code branches on a
provider id; adding Kagi is one JSON object.

Fields mirror Chromium's `TemplateURLData` (`search_url`, `suggest_url`, `keyword`, `encoding`,
`favicon_url`), so each entry converts 1:1 into a `TemplateURL` and Chromium's existing
`TemplateURLService`, OpenSearch discovery and keyword machinery keep working. We do **not**
build a parallel search stack.

**Default: DuckDuckGo.** Google is present and one click away, but a browser whose selling
point is autonomy should not ship a default that profiles the user. No search deal, ever —
that is what makes every other vendor's default what it is.

## Per-context engines

Chromium has exactly one default engine. Bedrock adds three optional overrides, each of which
may be empty meaning "inherit":

| Setting | Pref | Empty means |
|---|---|---|
| Default search engine | `bedrock.search.default` | — (must be set) |
| Normal search | `bedrock.search.normal` | default |
| Private window | `bedrock.search.private` | default |
| Address bar | `bedrock.search.omnibox` | normal, then default |
| Search suggestions | `bedrock.search.suggestions` | off |

Resolution lives in `bedrock/search/engine_selector.{h,cc}` — pure logic, host-tested
(`engine_selector_test.cc`). Rules it encodes:

1. An explicit bang beats every pref, in every context.
2. A pref pointing at an uninstalled engine falls through to the next candidate instead of
   leaving the user with a broken search box.
3. **Suggestions are forced off in private windows**, whatever the pref says: suggest requests
   ship every keystroke to the provider, which contradicts the mode. This is a hard rule in
   `SuggestionsAllowed()`, not a UI default.

## Shortcuts (bangs)

Every provider carries a `keyword` (`!g`, `!ddg`, `!br`, `!sp`, `!b`, `!w`) plus optional
`aliases`; users can edit them and add their own. Parsing is item 7
(`docs/design/007-omnibox.md`). Bangs work at the start *or* end of the input.

These are **local** shortcuts — Bedrock resolves them itself and sends the query straight to
the chosen provider. It does not proxy through DuckDuckGo's `!bang` redirector, which would
route the query through a third party the user did not pick.

## Settings → Search

Generated from the provider list: default + three context dropdowns, suggestions toggle,
provider table (name, keyword, search URL, edit/remove), "Add provider" (name + URL template
with `{searchTerms}` + keyword), and OpenSearch-discovered engines from visited sites.

## Wiring into Chromium (one patch, when the tree is checked out)

1. Replace the prepopulated set: patch `components/search_engines/prepopulated_engines.json`
   generation to read Bedrock's JSON, and neutralise `SearchEngineChoiceService` (the EU
   choice-screen machinery is Google-specific and useless once Google is not the default).
2. Register the four prefs in `chrome/browser/prefs/browser_prefs.cc`.
3. Call `SelectEngine()` from `ChromeAutocompleteProviderClient::GetDefaultSearchProvider()`
   equivalents for the private/omnibox contexts.
4. Add `//bedrock:bedrock` to `chrome/browser` deps.

Patch files land in `patches/bedrock/` once a checkout exists — writing them blind against
unknown line context would produce patches that only *look* real.
