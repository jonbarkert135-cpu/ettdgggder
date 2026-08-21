# Screen and window metrics

**Surface id:** `screen` · **Levels:** 0 allow · 1–3 normalize (letterboxing)

## Attack vector
`screen.width/height/availWidth/colorDepth`, `devicePixelRatio` and inner window size are high-entropy and, unlike most surfaces, survive across sites without any API call — the layout viewport alone leaks them.

## Mitigation
`QuantizeWindowSize()` rounds the reported viewport down to a bucket (50px at level 1, 100px at level 2, 200px at level 3) and the content area is letterboxed to that size with a neutral margin, so the *rendered* size matches the *reported* size — the Tor Browser approach. `screen.*` reports the quantised window rather than the physical display; `devicePixelRatio` is rounded to 1 or 2; `colorDepth` is pinned to 24.

## Compatibility impact
Visible margins around the page at levels 2–3 (the honest cost of the strongest anti-linking measure available). Responsive layouts behave normally since the viewport is a valid size. Multi-monitor and full-screen APIs report the quantised values.

## Performance impact
One layout pass on resize, same as any window resize.

## Test cases
- Reported and actual content size are equal at every level.
- Two machines with 1366×768 and 1440×810 displays report the same bucket at level 2.
- No quantisation at level 0.
- Tiny windows never report 0×0.
