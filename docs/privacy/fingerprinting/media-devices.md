# Media device enumeration

**Surface id:** `media-devices` · **Levels:** 0 allow · 1–2 normalize · 3 block

## Attack vector
`enumerateDevices()` lists cameras, microphones and speakers. Without permission, labels are already hidden, but the **number and kinds** of devices, and the stable `deviceId` salts, still fingerprint and can link sessions.

## Mitigation
Before permission is granted, level 1+ reports exactly one generic device per kind that actually exists, with `deviceId` derived from `SurfaceKey(..., kMediaDevices)` so it is stable per (site, session) and unlinkable across sites. After the user grants camera/mic access to a site, that site sees the real list — it needs it to let the user choose a device. Level 3 refuses enumeration entirely until permission is granted.

## Compatibility impact
Device pickers show one entry per kind until permission is granted, which is the normal flow anyway.

## Performance impact
None.

## Test cases
- Ungranted `enumerateDevices()` returns ≤1 device per kind.
- `deviceId` differs across sites, is stable within a session, and changes on restart.
- After granting, the real device list appears.
