# ADR 0002 — One matcher behind one interface: built-in C++ engine, adblock-rust as a swappable backend

**Status:** accepted (2026-08-21) · supersedes the "engine: adblock-rust" line in
`docs/design/008-privacy-engine.md`.

## Context

Earlier design docs named Brave's `adblock-rust` (MPL-2.0) as *the* content-blocking engine.
Roadmap item 12 asks for a real filter engine with the uBO feature set; item 13 forbids stacking
several independent blockers. Those two put a question on the table that the earlier docs
answered too early: what actually matches a request?

## Alternatives considered

Three options were on the table:

1. **adblock-rust only.** Mature, fast, MPL-2.0, Brave-scale battle testing. Costs: Rust +
   cbindgen/FFI in the Chromium build for every platform we target, a data model we do not
   control, and nothing runs in CI until a full Chromium checkout exists.
2. **Built-in C++ engine only.** No new toolchain, testable today on a host compiler, full
   control over the pipeline's data model. Costs: we own the long tail of filter syntax
   (regex rules, `badfilter`, `generichide`, scriptlet injection) that adblock-rust already has.
3. **One interface, two possible backends.**

## Decision

Option 3, with the built-in engine as the default.

`bedrock::blocking::FilterEngine` is the single matcher interface. Its current implementation is
ours (`filter_engine.cc`): ABP/uBO syntax, token-indexed network matching, cosmetic and
procedural rules, `$redirect` resources, user rules with import/export. Written from the
publicly documented filter syntax — no uBO code (GPL-3.0), no adblock-rust code.

`adblock-rust` stays a recorded, license-compatible option: adopting it means implementing the
same `FilterEngine` interface over the crate, and nothing above it changes — the pipeline, the
shields panel and the tests are written against the interface, not the implementation. It is
**not** in the tree at this commit (`docs/THIRD_PARTY.md` records it as a planned `vendored`
dependency).

The rule that keeps item 13 true: **exactly one backend is compiled in at a time.** Not one for
network rules and another for cosmetics, not a fallback chain.

## Consequences

- Blocking is testable and measurable today, with no Chromium and no Rust: the host test loads
  50k rules and asserts sub-20µs matching for a non-matching URL (measured ~0.2µs).
- We carry the syntax long tail. Unsupported syntax is *skipped at parse time*, never applied
  approximately — a rule we do not understand must not block something we did not intend.
- Regex filters (`/.../`) are deliberately unsupported: they defeat the token index and are the
  standard way to make a blocker slow. If real lists need them, that is the strongest argument
  for switching to backend 1 — and the switch is an implementation swap, not a rewrite.
- The decision is reversible on purpose, and reversing it does not touch the privacy pipeline.
