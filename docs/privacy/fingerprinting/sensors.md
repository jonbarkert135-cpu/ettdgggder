# Motion and environment sensors

**Surface id:** `sensors` · **Levels:** 0 allow · 1–3 block

## Attack vector
Accelerometer, gyroscope and magnetometer readings carry per-device calibration error that identifies a specific physical unit, not just a model. Ambient light and proximity leak the environment. None of it is needed by a desktop browser.

## Mitigation
Blocked from level 1: the Generic Sensor API and `devicemotion`/`deviceorientation` events are not delivered, and the interfaces are absent so feature detection takes the fallback path.

## Compatibility impact
Affects motion-controlled games and AR pages, mostly on mobile. Desktop impact ≈ zero, so this is on at level 1.

## Performance impact
None; fewer event dispatches.

## Test cases
- `window.DeviceMotionEvent` is undefined at level 1.
- No `devicemotion` events fire.
- Sensor API constructors throw the standard error.
