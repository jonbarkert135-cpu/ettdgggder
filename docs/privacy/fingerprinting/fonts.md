# Font enumeration

**Surface id:** `fonts` · **Levels:** 0 allow · 1–3 normalize

## Attack vector
Installed fonts are a strong, stable fingerprint. They leak through `document.fonts`, the Local Font Access API, and — silently — by measuring the rendered width of a string in a candidate font versus a fallback.

## Mitigation
From level 1 the renderer resolves fonts against a fixed allow-list: the fonts that ship with the platform, plus web fonts the page loads itself. A locally installed font that is not on the list is never matched, so measurement returns the fallback metrics for everyone. The Local Font Access API is refused. Level 3 additionally restricts the list to a minimal per-platform core set.

## Compatibility impact
Sites that name an exotic local font fall back to a standard one — a visual difference, not a functional one. Web fonts are unaffected, which is how most sites get their typography. Level 3 changes the look of pages relying on system fonts; flagged accordingly.

## Performance impact
The allow-list is a static hash set consulted during font matching. Negligible, and it slightly *reduces* work by skipping system font enumeration.

## Test cases
- Font-metric probe of 200 candidate fonts returns identical widths on two machines with different installed fonts.
- A page-loaded web font still renders.
- Local Font Access rejects without a permission prompt.
