# Timer resolution

**Surface id:** `timer-resolution` · **Levels:** 0 allow · 1–3 normalize (coarsen)

## Attack vector
High-resolution timers (`performance.now()`, `SharedArrayBuffer` counters) are the measuring instrument for *other* fingerprints — CPU timing, cache side channels, GPU rendering speed. Without coarsening, every hardware property becomes measurable indirectly.

## Mitigation
`performance.now()` and event timestamps are clamped to 100 µs at level 1, 1 ms at level 2, 100 ms at level 3, with jitter applied deterministically per (site, session) so repeated measurement cannot average the clamp away. Cross-origin isolated `SharedArrayBuffer` remains gated behind the standard headers.

## Compatibility impact
Frame-time profilers show coarse numbers; games and animation are unaffected because they use `requestAnimationFrame` timestamps, which stay usable. Level 3 is flagged: some benchmarks and audio-sync code misbehave.

## Performance impact
None.

## Test cases
- Successive `performance.now()` deltas are multiples of the clamp.
- Averaging 10,000 samples does not recover sub-clamp resolution.
- `requestAnimationFrame` still delivers ~60 fps pacing.
