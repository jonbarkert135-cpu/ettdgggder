# Architecture decision records

**Roadmap item 86.** Gate: `scripts/check_adr.py`.

An ADR records a decision that would otherwise be re-argued every six months, together with what
was rejected and why. It is written when the decision is made, not when it is questioned. ADRs
are immutable: a decision that changes gets a new record that supersedes the old one, and the old
one stays readable, because "why did they do it that way in 2026" is the question a new
contributor actually has.

Every record has the same five parts: **status**, **context**, **decision**, **alternatives
considered**, **consequences**. A record without alternatives is a description, not a decision.

| ADR | Decision | Roadmap |
| --- | --- | --- |
| [0001](0001-chromium-overlay.md) | Chromium as the base, consumed as an **overlay** rather than a fork | 1–3 |
| [0002](0002-filter-engine-backend.md) | One matcher interface; built-in C++ engine by default, adblock-rust swappable | 12, 13 |
| [0003](0003-source-layout.md) | Source layout: subsystem tree under `src_overrides/bedrock/` | 47 |
| [0004](0004-languages.md) | C++, Rust behind one FFI door, TypeScript in WebUI — no application shell over Chromium | 48 |
| [0005](0005-platform-abstraction.md) | Platform differences live behind one abstraction, no desktop environment assumed | 62–64 |
| [0006](0006-no-ui-frameworks.md) | No JavaScript UI framework or bundler; WebUI uses the platform | 78 |
| [0007](0007-privacy-architecture.md) | Privacy is one engine with a feature registry, not a bag of switches | 8, 11, 55 |
| [0008](0008-fingerprinting-strategy.md) | Deterministic per-site perturbation in four levels, not uniformity | 9, 10 |
| [0009](0009-search-architecture.md) | Provider-agnostic search, no paid default, classification before sending | 6, 7 |
| [0010](0010-tor-integration.md) | Tor mode with honest limits, no Tor branding, New Identity as one atomic act | 19, 22, 51 |
| [0011](0011-storage-isolation.md) | Storage partitioned by top-level site; unapproved third-party storage is ephemeral | 15, 20 |
| [0012](0012-theme-architecture.md) | Themes are tokens plus a manifest, validated for contrast, never code | 27–29 |
| [0013](0013-update-strategy.md) | Provider-agnostic updates, minimal check, security fixes on a clock | 40, 66, 69–71 |
| [0014](0014-license-strategy.md) | MPL-2.0 for own code; GPL studied, never linked; per-component licences kept | 4, 5, 50–53 |

## Mapping to the roadmap's own numbering

Item 86 lists ten decisions by its own numbers. Bedrock's files were numbered in the order the
decisions were actually made, which is not the same order — renumbering them would break every
existing reference, so the mapping lives here instead.

| Item 86 asks for | Recorded as |
| --- | --- |
| ADR-001 Chromium as base | [0001](0001-chromium-overlay.md) |
| ADR-002 Privacy architecture | [0007](0007-privacy-architecture.md) |
| ADR-003 Fingerprinting strategy | [0008](0008-fingerprinting-strategy.md) |
| ADR-004 Content blocker | [0002](0002-filter-engine-backend.md) |
| ADR-005 Search architecture | [0009](0009-search-architecture.md) |
| ADR-006 Tor integration | [0010](0010-tor-integration.md) |
| ADR-007 Storage isolation | [0011](0011-storage-isolation.md) |
| ADR-008 Theme architecture | [0012](0012-theme-architecture.md) |
| ADR-009 Update strategy | [0013](0013-update-strategy.md) |
| ADR-010 License strategy | [0014](0014-license-strategy.md) |

Three decisions have records because they came up, not because the list asked for them: source
layout (0003), languages (0004) and platform abstraction (0005), plus the UI framework ban (0006).

## Writing a new one

1. Take the next number. Never reuse or renumber.
2. Use the five sections. If the alternatives section is hard to fill, the decision is probably
   not yet made — go and find the option someone will propose in six months.
3. Add a row to both tables above if the roadmap asked for it.
4. `python3 scripts/check_adr.py` fails on a missing section, a record absent from this index, or
   an index entry with no file.
