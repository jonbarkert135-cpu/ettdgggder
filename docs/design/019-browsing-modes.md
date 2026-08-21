# 019 — Browsing modes and the Tor transport

**Roadmap item 19.** Status: landed and host-tested
(`src_overrides/bedrock/session/browsing_mode.{h,cc}`).

## Tor is a transport mode, not a personality

Bedrock does not become the Tor Browser when Tor is switched on, and it never routes everything
through Tor by default. The user opens a **Tor window**; that window uses the network, the rest
of the browser is unchanged. A browser that quietly sends all traffic through Tor is slower than
the user expects and more dangerous than they assume — they will do things in it that Tor cannot
protect.

| | Normal | Private | Tor |
|---|---|---|---|
| storage | persistent, partitioned | ephemeral | ephemeral |
| history | kept | not recorded | not recorded |
| fingerprinting | Balanced (per-site override) | Balanced | **Maximum, locked** |
| DNS | profile setting | profile setting | resolved at the exit node |
| WebRTC | mode setting | mode setting | **off** |
| proxy unreachable | n/a | n/a | **fail closed** |

Two of those deserve a reason:

- **Fingerprinting is locked at Maximum in Tor mode.** A Tor user who looks unique has given up
  the property the mode exists for, so a per-site "turn protection down" cannot apply here.
- **WebRTC is off, not restricted.** It can bypass the proxy in ways Chromium does not fully
  control, and one leaked address defeats the whole transport.

## Circuit isolation

Streams are isolated with SOCKS credentials — the same mechanism Tor Browser uses:

```
username = top-level site
password = identity epoch
```

Same site → same circuit, so the site keeps working. Different site → different circuit, so two
sites cannot be correlated at the exit. New Identity or a mode switch bumps the epoch, and every
future stream gets a fresh circuit. Switching modes is a session boundary, not a continuation:
tested.

## Never say "anonymous"

Every user-visible string in the mode layer is checked by a test against a banned list:
*anonymous, anonymity, untraceable, 100%, completely private, no one can, invisible*. The status
text is **"Privacy protection enabled"**, exactly as roadmap item 19 requires.

The ban is absolute, including inside denials such as "does not make you untraceable" — a string
scanner cannot judge negation, and a screenshot of a sentence containing the word travels further
than the sentence does.

Tor windows also show their limitations up front, not in a help page: signing in identifies you,
downloaded files can phone home when opened, Bedrock is **not** the Tor Browser and has not been
reviewed by the Tor Project, sites block Tor traffic, and an observer watching both ends can
still correlate.
