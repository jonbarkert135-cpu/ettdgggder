# Screen and window metrics

**Surface id:** `screen` · **Levels:** 0 allow · 1–3 normalize (letterboxing)

## Attack vector
`screen.width/height/availWidth/colorDepth`, `devicePixelRatio` and inner window size are high-entropy and, unlike most surfaces, survive across sites without any API call — the layout viewport alone leaks them.

## Mitigation
`QuantizeWindowSize()` rounds the reported viewport down to a bucket (50px at level 1, 100px at level 2, 200px at level 3) and `ComputeLetterbox()` (`privacy/fingerprinting/letterboxing`, design doc [050](../../design/050-letterboxing.md)) renders the page into that box, centred, with the leftover pixels as a neutral margin — so the *rendered* size is the *reported* size, which is the whole point of the mechanism. There is no fullscreen exception, the margin split never varies, and the box does not depend on the site. Two guards keep it usable: a 200x100 floor and a "page keeps at least 60% of the pixels" share; below either, the window is left alone and the protection is reported as inactive. `screen.*` reports the content box rather than the physical display; `devicePixelRatio` is rounded to 1 or 2; `colorDepth` is pinned to 24.

## Compatibility impact
Visible margins around the page at levels 2–3 (the honest cost of the strongest anti-linking measure available). Responsive layouts behave normally since the viewport is a valid size. Multi-monitor and full-screen APIs report the quantised values.

## Performance impact
One layout pass on resize, same as any window resize.

## Test cases
- Reported and actual content size are equal at every level.
- Two machines with 1366×768 and 1300×760 windows report the same bucket at level 3 (asserted in `letterboxing_test.cc`; note that quantisation merges neighbours, it does not merge every pair).
- No quantisation at level 0.
- Tiny windows never report 0×0, and a window below either guard keeps its real size.
- A resize inside one bucket changes nothing the page can observe.
- Fullscreen is letterboxed like any other window of that size.
