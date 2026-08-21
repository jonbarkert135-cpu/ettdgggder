# WebRTC

**Surface id:** `webrtc` · **Levels:** 0 allow · 1–2 normalize · 3 block

## Attack vector
ICE candidate gathering exposes local IP addresses (and, historically, the real IP behind a VPN) without any prompt. Enumerated codecs and their order also fingerprint the build and hardware.

## Mitigation
From level 1 the ICE policy hides local candidates: host candidates are replaced by mDNS-style obfuscated identifiers and only the public candidate is offered. The codec list is normalized to a standard set in a fixed order. Level 3 disables WebRTC after a prompt.

## Compatibility impact
Peer-to-peer calls still work over the public candidate; some LAN-only apps lose direct connections and fall back to a relay.

## Performance impact
Slightly slower connection setup where a LAN path is no longer discovered.

## Test cases
- No RFC1918 address appears in any candidate at level 1.
- A basic call between two peers still connects.
- Codec list identical across machines.
