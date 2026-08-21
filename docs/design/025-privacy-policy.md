# 025 — PrivacyPolicy: the single source of truth

**Roadmap item 25.** Status: landed and host-tested
(`src_overrides/bedrock/privacy/privacy_policy.{h,cc}`).

```
PrivacyPolicy
 ├── TrackingProtection      block_ads, block_trackers, behavioral_detection
 ├── FingerprintProtection   fingerprint, fingerprint_locked
 ├── CookiePolicy            cookies
 ├── StoragePolicy           storage
 ├── NetworkPolicy           dns, strip_tracking_params, send_privacy_signals
 ├── WebRTCPolicy            webrtc
 ├── PermissionPolicy        permissions
 ├── ReferrerPolicy          referrer
 ├── ScriptPolicy            scripts
 └── SecureConnectionPolicy  https
```

Every layer already existed. What was missing was the guarantee that they **agree**. Without one
resolver, the fingerprinting shim can sit at Maximum while third-party storage stays persistent,
or a Tor window can inherit a per-site exception that turns protection off. Each subsystem is
individually correct; the browser is wrong.

## Precedence, written once

```
1. Mode floor      Tor / Private impose minimums nothing may lower
2. Site override   the shields panel
3. Domain override
4. Profile default
```

The mode floor is applied **last**, because it is a floor. A per-site exception is a decision the
user made about a normal window; carrying it into a Private or Tor window would silently import
their convenience choice into the context where they expect the opposite.

## Invariants, checked

`Conflicts()` re-validates a finished resolution and must always return empty. The test drives
**84 combinations** (3 modes × 7 controls × 4 values) and asserts exactly that, plus a
deliberately broken resolution to prove the checker is not vacuous. Current invariants:

- fingerprinting at Maximum ⇒ third-party storage is not persistent;
- Private/Tor ⇒ storage is not persistent;
- Tor ⇒ WebRTC locked, fingerprinting locked, HTTPS-only, cookies restricted;
- behavioral detection agrees with the tracker setting — one may not block what the other allows;
- link cleaning and the GPC/DNT signal follow the user's tracker setting rather than contradicting it.

Writing them down found a real bug: setting fingerprinting to Maximum for a single site left
storage persistent, so the shims were being defeated by a cookie. The resolver now raises storage
to ephemeral third-party in that case — a cross-layer rule that, before this class, was nobody's
job.

## The panel is generated

`Explain()` produces the per-layer sentences from the resolution, so the shields UI cannot drift
away from what the engine actually does.
