# navigator.deviceMemory

**Surface id:** `device-memory` · **Levels:** 0 allow · 1–3 normalize

## Attack vector
Coarse RAM size, already quantised by spec to a power of two capped at 8 — still 4–5 distinguishable buckets that combine with core count.

## Mitigation
Level 1–2 report `min(actual, 8)`; level 3 pins `4`, the population mode.

## Compatibility impact
Sites use it to pick a lite mode; the effect is at most a slightly lighter experience.

## Performance impact
None.

## Test cases
- 32 GB machine reports 8.
- Level 3 reports 4 on every machine.
