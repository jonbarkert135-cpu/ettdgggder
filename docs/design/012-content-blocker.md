# 012 — Content blocker

**Roadmap item 12.** Status: engine landed and host-tested
(`src_overrides/bedrock/blocking/filter_engine.{h,cc}`).

## What we studied, and what we may use

uBlock Origin's architecture is public and worth learning from: filters are parsed into typed
buckets, indexed by a **token** extracted from the pattern, and a request is matched by
tokenizing its URL and examining only the filters that share a token. That is what keeps a
300k-rule list fast; it is an algorithmic idea, described in uBO's own documentation and wiki.

uBO's **code** is GPL-3.0 and cannot enter an MPL-2.0 tree
(`THIRD_PARTY_NOTICES/ublock-origin.txt`). So: syntax and algorithm reimplemented from the
public documentation, zero lines copied, no uBO assets, no uBO lists compiled in. Filter lists
are separate works fetched at runtime from their authors under their own licenses.

Engine choice (built-in C++ vs Brave's `adblock-rust`) is [ADR 0002](../adr/0002-filter-engine-backend.md).

## Supported

| Requirement | How |
|---|---|
| network filtering | `||domain^`, `|start`, `end|`, `*`, `^`, substring patterns |
| options | `$script,image,xhr,subdocument,stylesheet,font,media,websocket,ping,document,other`, negated (`~script`), `third-party`/`first-party`, `domain=a.com\|~b.com`, `important`, `redirect=` |
| allowlists | `@@` exception rules; `$important` beats them (and nothing beats `$important`) |
| blocklists | any number of lists via `AddList()`, parsed independently |
| cosmetic filtering | `domain##selector`, generic `##selector`, exceptions `#@#` |
| procedural rules | `:has-text()`, `:has()`, `:upward()`, `:xpath()`, `:matches-css` — parsed, flagged and returned separately for the JS evaluator, never injected as CSS |
| redirect resources | built-in inert stand-ins (`noopjs`, `noopframe`, `1x1.gif`, `2x2.png`, `nooptext`) |
| per-site / global / dynamic rules | `AddRule()` / `RemoveRule()`, resolved together with the Protection Controller's scopes |
| import/export | `ExportRules()` emits a filter list containing the user's rules only; it round-trips through `AddList()` |

**Deliberately not supported:** regex filters (`/.../`) and options we cannot enforce
(`generichide`, `badfilter`, `csp=`). An unparsable rule is *skipped*, never approximated —
applying a rule we half-understand is how a blocker breaks a bank site.

## Performance

The cost of a request must not scale with list size:

- one token per filter, chosen as the longest **word-aligned** literal in the pattern (uBO picks
  by measured frequency; alignment is the part that matters for correctness, and length is a
  cheap stand-in for rarity that needs no histogram in the binary);
- filters whose token cannot be word-aligned go to a small always-checked bucket, so they are
  slow but never silently wrong;
- matching tokenizes the URL once and probes only the matching buckets;
- no allocation on the hot path beyond one token vector; no regex engine anywhere.

The host test loads **50,000 rules** and asserts **< 20 µs** per non-matching URL; measured
**~0.2 µs** on CI hardware. A linear scan would be ~1000× slower, so the assertion fails
immediately if someone replaces the index with a loop.

## Where it plugs in

`FilterEngine` decides nothing on its own — it answers "does a rule match?" for the single
[blocking pipeline](013-blocking-pipeline.md). Cosmetic selectors are delivered to the renderer
per document host; procedural ones go to the content-side evaluator.
