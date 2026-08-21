# Aggressive JS restrictions

**Surface id:** `js-restrictions` · **Levels:** 0–1 allow · 2 normalize · 3 block

## Attack vector
Some APIs are only used, in practice, to fingerprint or to attack: `SharedArrayBuffer` timing loops, WebAssembly threads, WebUSB/WebHID/WebSerial/WebBluetooth device enumeration, `navigator.connection`, `navigator.getInstalledRelatedApps`, `Notification` before interaction.

## Mitigation
Level 2 gates device APIs (USB/HID/Serial/Bluetooth) behind an explicit prompt and normalizes `navigator.connection`. Level 3 mirrors Tor Browser's highest security level: JIT-sensitive and device APIs off, WebAssembly off, `SharedArrayBuffer` off, remote fonts off, media click-to-play.

## Compatibility impact
Level 3 is openly labelled *«breaks many sites»* in the UI, with the same honesty as Tor Browser's security slider — the user is told before switching, and the shields panel offers per-site relief.

## Performance impact
Level 3 makes WASM-heavy apps unusable, which is intended.

## Test cases
- Device APIs prompt at level 2.
- WebAssembly is undefined at level 3.
- The level-3 warning is shown before the switch takes effect.
