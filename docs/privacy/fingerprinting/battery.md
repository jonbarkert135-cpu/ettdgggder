# Battery Status API

**Surface id:** `battery` · **Levels:** 0 allow · 1–3 block

## Attack vector
Charge level (a double with fine granularity) plus charging/discharging times form a short-lived but cross-site identifier: two sites reading the same values within seconds know it is the same device. This is why the API was removed from Firefox and restricted elsewhere.

## Mitigation
`navigator.getBattery` is absent from level 1. There is no legitimate desktop use worth the leak.

## Compatibility impact
Effectively none; a handful of web apps reduce activity on low battery and lose that ability.

## Performance impact
None.

## Test cases
- `navigator.getBattery` is undefined at level 1.
- Feature detection takes the fallback path without throwing.
