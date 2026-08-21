# Defaults and the four axes of control

**Roadmap items 83 and 84.** Gate: `scripts/check_defaults.py`.
Code: [`src_overrides/bedrock/settings/defaults.cc`](../src_overrides/bedrock/settings/defaults.cc).

## The shipped configuration: Balanced Privacy

A fresh profile starts at **Balanced Privacy** on every axis. Not the maximum of any of them.

The reasoning is not modesty. A browser configured to the strongest setting of everything
produces a first week of broken checkouts and missing videos, and the user's fix is not to find
the one responsible switch — it is to turn the whole thing off, or to go back to Chrome. A
default that survives contact with the user's actual life protects them for years; a default
that impresses a reviewer protects them for a week.

So: strong enough that someone who never opens Settings is materially better off, mild enough
that they never have to.

| Setting | Default | Why |
| --- | --- | --- |
| Secure browsing | enabled | Site isolation, sandboxing and the item 24 security baseline. There is no user for whom exploitation is acceptable. |
| HTTPS upgrades | enabled | Upgrading is nearly free. HTTPS-*only* (fail instead of fall back) is the Strict choice, because unreachable local devices are a real cost. |
| Third-party tracking protection | enabled | The largest privacy gain available without breaking the web, and the one users cannot achieve for themselves. |
| Third-party cookies | restricted | Restricted, not blocked: blocking outright breaks third-party login flows people depend on. |
| Fingerprint protection | balanced | Level 1: normalise what is free to normalise, perturb canvas per site, leave the aggressive measures to Strict. |
| Ad and tracker blocking | enabled | Blocking at the network layer is faster than loading and hiding, and closes a common malware path. |
| Telemetry | **disabled** | Not a trade-off. There is no reporting machinery to enable (item 39). |
| Crash reporting | **disabled** | Reports stay local; upload needs per-report consent (item 81). |
| WebRTC privacy | enabled | Local IP addresses are not exposed. Almost no site needs host candidates. |
| Secure DNS | configurable | Deliberately not forced: encrypted DNS moves trust to a resolver the user did not pick, and breaks captive portals. Bedrock asks. |
| Extension permissions | explicit | What it asked for at install, shown as a capability disclosure; an update can never widen it (item 23). |
| Site permissions | ask when needed | Camera, microphone, location, notifications, clipboard — asked at the moment of use, never pre-granted, never inherited by an embedded frame. |

## The four axes

The user chooses along four independent axes. Independent is the important word: someone who
wants a dark theme has not asked for weaker protection, and someone who wants a faster browser
has not asked to be tracked.

| Axis | Choices | Default |
| --- | --- | --- |
| Privacy | Standard · **Balanced** · Strict | Balanced |
| Compatibility | Strict · **Balanced** · Maximum | Balanced |
| Performance | Efficiency · **Balanced** · Speed | Balanced |
| Appearance | **System** · Light · Dark | System |

Two settings are on **no** axis: telemetry and crash-report upload. There is no compatibility or
performance argument for either, so offering them as a trade-off would be theatre. The host test
walks every axis and every choice and fails if any of them touches those two.

## No axis weakens protection quietly

Every effect of a choice is enumerated in code as a `Change`, and one that reduces a protection
carries `weakens_protection` and an explanation in the user's terms. The settings UI shows those
lines in the confirmation, so "I want fewer broken sites" can never turn into "I silently
accepted third-party cookies".

For example, moving compatibility to Maximum lists:

* `third_party_cookies: restricted → allowed` — *weaker.* "This is the setting most often behind
  a broken checkout — and the one trackers use."
* `ad_and_tracker_blocking: enabled → trackers_only` — *weaker.* "Cosmetic filtering stops, so
  pages look as their authors intended, ads included."

while moving performance to Speed lists only `preload_and_prerender`, which changes no
protection at all — the performance axis moves rendering and background work, never privacy.

## When a default contradicts its own score

`docs/privacy/TRADEOFFS.md` scores every protection on five axes (item 85). Four features ship
**on** although the arithmetic says opt-in; each of them carries a written argument in the source,
reproduced in that document, and the host test fails if one is missing. That is the intended
relationship between the two documents: the score is the default answer, an exception is allowed,
and an exception without a reason is not.

Related: [`CONFIGURATION.md`](CONFIGURATION.md) (every setting across GUI, config, policy and CLI),
[`privacy/FEATURES.md`](privacy/FEATURES.md), [`privacy/TRADEOFFS.md`](privacy/TRADEOFFS.md).
