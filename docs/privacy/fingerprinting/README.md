# Anti-fingerprinting: per-surface documentation

Architecture and level model: [`../../design/010-anti-fingerprinting.md`](../../design/010-anti-fingerprinting.md).

One file per protected surface, each with attack vector, mitigation, compatibility impact,
performance impact and test cases. `scripts/check_fp_docs.py` fails CI if a surface in
`fingerprint_policy.cc` has no doc here, if a doc is missing a required section, or if a doc
describes a surface that no longer exists.

Two rules every entry follows:

1. **Normalize before perturbing.** Looking like everyone else beats looking different.
2. **Never random per call.** Values come from a pure function of (session secret, site,
   surface) — stable within a session, unlinkable across sites and sessions.

- [`audio`](audio.md)
- [`battery`](battery.md)
- [`canvas`](canvas.md)
- [`client-hints`](client-hints.md)
- [`device-memory`](device-memory.md)
- [`fonts`](fonts.md)
- [`gamepad`](gamepad.md)
- [`hardware-concurrency`](hardware-concurrency.md)
- [`js-restrictions`](js-restrictions.md)
- [`language`](language.md)
- [`media-devices`](media-devices.md)
- [`plugins`](plugins.md)
- [`screen`](screen.md)
- [`sensors`](sensors.md)
- [`speech-voices`](speech-voices.md)
- [`storage-isolation`](storage-isolation.md)
- [`timer-resolution`](timer-resolution.md)
- [`timezone`](timezone.md)
- [`user-agent`](user-agent.md)
- [`webgl`](webgl.md)
- [`webrtc`](webrtc.md)
