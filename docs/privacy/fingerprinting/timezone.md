# Timezone

**Surface id:** `timezone` · **Levels:** 0–1 allow · 2–3 normalize

## Attack vector
`Date.getTimezoneOffset()` and `Intl.DateTimeFormat().resolvedOptions().timeZone` reveal the zone, which narrows location and links sessions when combined with language.

## Mitigation
From level 2 the renderer reports UTC and formats all dates in UTC. It is deliberately **not** applied at level 1: it makes every calendar, booking and meeting site show wrong local times, which is a failure mode users cannot diagnose.

## Compatibility impact
Significant and flagged: displayed times are UTC on sites that use the browser zone. The shields panel offers a one-click per-site exception.

## Performance impact
None.

## Test cases
- Offset is 0 and the zone string is `UTC` at level 2.
- `Date` formatting and `Intl` agree.
- Level 1 still reports the real zone.
