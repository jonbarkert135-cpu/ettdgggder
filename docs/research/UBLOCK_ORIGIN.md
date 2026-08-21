# uBlock Origin research

**Roadmap item 52.** uBO's architecture, what a differently licensed browser may take from it,
and the licensing of the filter lists — which is a separate question with separate answers per
list.

**Sourcing, honestly.** Compiled from uBO's public wiki and documentation (filter syntax,
static/dynamic/cosmetic filtering, the request pipeline) — not from a code read in this
repository's CI. That distinction matters more here than anywhere else: uBO is **GPL-3.0-or-later**
and Bedrock is MPL-2.0, so reading its code for implementation guidance is precisely what we
must not do.

## Licence position — the hard boundary

- **uBO's code: GPL-3.0-or-later.** Not vendored, not linked, not ported, not translated.
  Linking it would relicense the browser binary. This is already recorded in
  [`docs/LICENSING.md` §3](../LICENSING.md) and enforced socially by the rule that every
  blocking file carries a header saying it was written from the documented syntax.
- **What is allowed:** implementing the *documented filter syntax* (a format, not code), which
  is what `privacy/tracker_blocker/filter_engine.cc` does; supporting uBO as a user-installed
  WebExtension (aggregation, not derivation); and using `adblock-rust` (MPL-2.0) as a backend.
- **Filter lists are separate works with separate licences** — see the inventory below and
  [`docs/privacy/FILTER_LISTS.md`](../privacy/FILTER_LISTS.md).

## Architecture, mechanism by mechanism

| uBO mechanism | What it is | Verdict for Bedrock | Notes |
| --- | --- | --- | --- |
| **Static network filtering** | ABP-syntax rules compiled into token-indexed buckets, matched per request | **Reimplemented** (item 12). Syntax compatible, code independent. | our engine, our data structures |
| **Token selection** | uBO picks the filter's **least frequent** token, using a frequency histogram of the loaded lists | **Independently arrived at the same answer** in item 46: we indexed on the *longest* token, measured 70 µs on a matching request, switched to the *rarest* token and got 0.21 µs (~330×). Worth stating plainly: the design literature agreed with the measurement, and the measurement is what found the bug. | — |
| **Options / modifiers** (`$third-party`, `$domain`, `$important`, `$badfilter`, …) | per-rule qualifiers, including rules that cancel other rules | **Supported syntax-side**; `$badfilter` is the subtle one — it edits the rule set rather than the match result, and must be applied at compile time. | correctness risk if applied at match time |
| **`$redirect` / resource replacement** | serve a neutral stand-in instead of blocking, so pages keep working | **Present in our engine**; the *resource catalogue* is the ongoing work (also reachable from brave-core, MPL-2.0). | maintenance promise |
| **Dynamic filtering** | a per-scope matrix (site × party × type ⇒ block/allow/noop) evaluated before static rules | **Adopt the model, fold into the one pipeline.** This is uBO's most powerful feature and the easiest way to end up with two blockers — it must be a *stage* of `BlockingPipeline::Evaluate()` (item 13), never a second decision-maker. Also the place where a UI can create rules a user cannot debug: any adoption needs a "why was this blocked" answer. | medium-high |
| **Cosmetic filtering** (`##selector`) | element hiding by CSS selector, per-domain | **Present.** Generic vs. specific cosmetic rules have very different costs; `generichide` matters for performance. | — |
| **Procedural cosmetic filtering** (`:has()`, `:matches-css()`, `:xpath()`) | selectors evaluated in JS because CSS cannot express them | **Adopt selectively.** These run in the page and cost CPU on every DOM mutation — a performance budget item (item 46), not a free feature. | perf budget required |
| **Scriptlet injection** (`//#`) | small scripts neutering specific page behaviours | **Adopt the mechanism, write our own scriptlets.** The scriptlet *library* is GPL-3.0 code — the most tempting and most clearly forbidden thing in this project. | licence trap |
| **Request pipeline** | extension `webRequest` blocking hooks, plus CSP injection and (Firefox-only) HTML stream filtering | **Not our shape.** Bedrock blocks in the browser, not in an extension (item 13); Chromium MV3 has no blocking `webRequest` anyway. CSP injection as a blocking tool is worth evaluating. | see the MV3 question in the Firefox research |
| **Filter list handling** | subscriptions, update cadence, `!#include`, `!#if` directives, compiled-list caching | **Adopt the mechanics**, including `!#include`/`!#if` (lists in the wild use them, so a parser without them silently mis-parses). Compiled-list caching is what keeps startup cheap; it is also a `pending` performance budget of ours. | parser must be fuzzed — it already is |
| **"My rules" / user rules with import-export** | user-authored rules as a first-class, portable list | **Present** (item 12), and it stays first-class: a blocker the user cannot override is a blocker they cannot trust. | — |
| **Performance model** | tokenisation + rarest-token bucketing + early exit; work proportional to matching candidates, not to rule count | **Same model, measured** — our budgets print match time per commit and fail the build on regression. | — |

## Filter list licences — one per list, never one for the set

Bedrock **bundles no filter list**. Lists are fetched at runtime, by the user's machine, from
the list authors' own URLs (item 12, invariant 1). That is a licensing decision as much as an
autonomy one: shipping a GPL-3.0 list file inside our package is a distribution of GPL-3.0
material with all its obligations, and doing so for a default set nobody audited is how a
project acquires an obligation it did not read.

The per-list inventory, the rule that a list may not become a *default* subscription until its
licence has been checked and recorded, and the gate that enforces both live in
[`docs/privacy/FILTER_LISTS.md`](../privacy/FILTER_LISTS.md).

## What this changes now

Nothing in the code. Recorded follow-ups:

1. **Dynamic filtering as a pipeline stage**, with a "why was this blocked" explanation path.
2. **`!#include` / `!#if` support** in the list parser, with fuzz coverage extended to them.
3. **Procedural cosmetic filtering** behind its own performance budget.
4. **Our own scriptlet catalogue** — mechanism ours, content written here, never copied.

## What we will not take

- uBO code, in any amount, in any language, including "translated" versions of it.
- uBO's scriptlet and resource libraries (GPL-3.0).
- Any filter list shipped inside the browser package.
- The claim that Bedrock "is" uBlock Origin: the syntax is compatible, the implementation is
  not the same, and behaviour will differ at the edges. Say that, rather than implying parity.
