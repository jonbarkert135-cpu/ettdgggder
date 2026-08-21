# 011 — Protection Controller (shields)

**Roadmap item 11.** Status: resolver landed and host-tested
(`src_overrides/bedrock/privacy/core/protection_controller.{h,cc}`).

## What we studied

Brave Shields groups per-site protections behind one panel: ads/trackers, HTTPS upgrades,
script blocking, fingerprinting level, third-party cookies, referrer and query-parameter
handling, all overridable per site. The idea worth taking is the **panel as the single control
surface**: one place that both shows what happened on this page and changes it.

What we did not copy is the code — see `THIRD_PARTY_NOTICES/brave-core.txt`. No Brave code is
in the tree; `adblock-rust` is a recorded, license-compatible option for the filter backend
(ADR 0002), not a dependency at this commit.

## Model: three scopes, one rule

```
site (news.example.com)  →  domain (example.com)  →  global  →  built-in default
```

Most specific wins; anything unset inherits. That is the entire resolution algorithm
(`ProtectionController::Get`). No priorities, no rule ordering, no per-control exceptions —
every special case in a security-relevant resolver is a future hole.

`EffectiveScope()` reports **which** scope decided a value, so the panel can say "set for this
site" / "set for example.com" / "default" instead of showing a toggle whose origin is a mystery.

`Clear()` removes overrides rather than writing the current value, so reset restores
inheritance — tested explicitly, because "reset" that freezes the resolved value is a classic bug.

## Controls and defaults

| Control | Default | Allow | Reduce | Block |
|---|---|---|---|---|
| Ads | Block | off | standard lists | aggressive lists |
| Trackers | Block | off | standard lists | + heuristic blocking |
| Fingerprinting | Reduce (Level 1) | Level 0 | Level 1 | Level 2 (`BlockStrict` = Level 3) |
| Cookies | Reduce | all allowed | third-party blocked | all blocked |
| Scripts | Allow | all | third-party only | all blocked |
| HTTPS | Reduce | off | upgrade, fall back | HTTPS-only |
| Referrer | Reduce | full | origin only | none |

Scripts default to **Allow**: blocking JS breaks most of the web, and a default that forces
users to disable shields on every site trains them to disable shields. Everything else defaults
to real protection.

A single `Value` ladder (`Allow / Reduce / Block / BlockStrict`) is shared by all controls
rather than seven bespoke enums — the UI renders one three-position control per row, and prefs
stay one integer per (scope, control).

## Panel

Per site: the seven controls, the counts of what was blocked (from the local action log), the
resolved fingerprinting level, "apply to whole domain", and a reset. A "Report broken site"
entry opens a prefilled GitHub issue — it does not send anything anywhere by itself, since
Bedrock has no server (see item 4).

## Storage

Overrides live in the profile's content settings store, keyed by scope, so they are backed up,
exported and cleared with the rest of the profile and never leave the device.
