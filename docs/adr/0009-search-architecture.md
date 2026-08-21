# ADR 0009 — Search: provider-agnostic, no default-search deal, ever

**Status:** accepted (2026-08-21) · roadmap items 6, 7 · owner's list: ADR-005
**Design notes:** `docs/design/006-search-system.md`, `docs/design/007-omnibox.md`

## Context

Default search placement is how browsers are funded. It is also the mechanism by which a browser's
incentives stop matching its users': the party paying for the default is the party the browser is
then reluctant to protect you from. Every privacy browser that took the money has had to explain
it afterwards.

At the same time, search is the most-used feature in the product, and the omnibox is where a typo
becomes a query sent to a third party.

## Decision

1. **No paid default.** Bedrock will not accept payment for search placement. This is recorded
   here rather than in a marketing page so that reversing it requires superseding an ADR.
2. **Provider-agnostic engine model.** A search provider is data — name, endpoints, suggestion
   behaviour, an explicit privacy note — and adding one requires no code. Google, DuckDuckGo,
   Startpage, Brave Search, Mojeek and a fully custom entry are all the same kind of object.
3. **The omnibox classifies before it sends.** `omnibox::InputParser` decides locally whether
   input is a URL, a search or ambiguous. Anything that parses as a URL is never sent to a search
   provider — the classic leak where an intranet hostname or a pasted private link ends up in a
   query log.
4. **Suggestions are off unless chosen.** Live suggestions mean every keystroke reaches a third
   party. Bedrock asks first, per provider, and says plainly what turning them on means.

## Alternatives considered

* **Bundling a privacy-preserving proxy in front of a big engine.** Attractive, and it makes
  Bedrock the party seeing every query. Rejected: that is a server, and Bedrock has none (item 39).
* **Shipping one privacy-branded default and no choice.** Rejected: the choice is the feature.

## Consequences

* Funding cannot come from search. Any future funding model has to be stated in `README.md` and
  survive the same scrutiny.
* Provider-specific quirks (suggestion formats, POST-only endpoints) live in data, so a broken
  provider is a data fix.
