# Canvas

**Surface id:** `canvas` · **Levels:** 0 allow · 1–2 farble · 3 block

## Attack vector
`toDataURL()`, `getImageData()` and `measureText()` expose sub-pixel differences produced by the GPU, driver, font rasteriser and anti-aliasing stack. The resulting hash is stable and high-entropy — the classic canvas fingerprint, in commercial use since 2012.

## Mitigation
At levels 1–2, readback is perturbed: for each read, at most a few least-significant bits per channel are altered, chosen by `SeededUnit(SurfaceKey(session_secret, eTLD+1, kCanvas), pixel_index)`. Consequences of that construction:

- the same site reading the same canvas twice gets **identical** bytes, so averaging over repeated reads does not recover the true value;
- a different site gets a different perturbation, so the hash cannot link across sites;
- a new session gets a new secret, so it cannot link across sessions.

Only *readback* is perturbed — what is displayed on screen is untouched. Level 3 makes reads throw / return a blank surface after a user prompt.

## Compatibility impact
Levels 1–2: invisible to humans; breaks only code that compares canvas bytes to a reference (rare, mostly anti-bot vendors). Level 3 breaks image editors, signature pads, chart exporters and games that read pixels.

## Performance impact
One multiply-add per touched pixel on **read only**; drawing is untouched. Reads are already slow (GPU→CPU sync), so the added cost is in the noise.

## Test cases
- Same canvas read twice in one page load → byte-identical.
- Same canvas across two page loads on the same site in one session → identical.
- Same canvas on two different sites → different.
- Same canvas after restart → different.
- Rendered output on screen unchanged (pixel-compare against level 0 display buffer).
