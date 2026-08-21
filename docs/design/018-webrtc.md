# 018 — WebRTC

**Roadmap item 18.** Status: landed and host-tested
(`src_overrides/bedrock/privacy/network/webrtc_policy.{h,cc}`).

## The problem

WebRTC gathers ICE candidates to find the shortest path between peers, and those candidates
contain IP addresses. A page can read them with **no permission prompt and no call in progress** —
the well-known WebRTC leak, which also reveals the address behind a VPN in the default
configuration.

## Modes

| Mode | Chromium `WebRTCIPHandlingPolicy` | Without media permission | With permission |
|---|---|---|---|
| Default | `default` | all interfaces | all interfaces |
| **Privacy (default)** | `disable_non_proxied_udp` → `default_public_interface_only` | nothing gathered | public interface only, mDNS for local |
| Strict | `disable_non_proxied_udp` | nothing gathered | proxy only; direct calls refused |

Chromium exposes exactly one lever here. Pretending in the UI that we have finer control than the
engine does would be a lie, so the modes map onto that lever and say so.

Two decisions worth naming:

- **Permission is the gate.** Before a site has camera or microphone access it has no reason to
  gather candidates at all, so in Privacy mode it gets the narrowest policy available.
- **Permission is not a licence to read the LAN.** Granting camera access lets a site place a
  call; it does not entitle it to the local network layout. `ExposesLocalAddresses()` ignores the
  permission flag entirely — only `Default` ever hands out a real local IP, and mDNS obfuscation
  covers the rest.

## Honest UI

Every mode ships `Explain()` (the risk) and `Tradeoff()` (the cost), and the tests assert both
are real sentences. The explanations state what is **not** hidden: the public IP is visible in
every mode during a call, because the other side of the connection needs it. A privacy control
that implies it hides more than it does is worse than no control.
