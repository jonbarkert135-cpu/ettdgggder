# Language

**Surface id:** `language` · **Levels:** 0 allow · 1–3 normalize

## Attack vector
`navigator.language`, `navigator.languages` and the `Accept-Language` header leak locale and often region. `languages` with several entries in a specific order is highly identifying, and it correlates with the user's real-world identity, not just their machine.

## Mitigation
From level 1, `navigator.language(s)` returns `en-US` and `Accept-Language` sends `en-US,en;q=0.9`, regardless of UI language. The browser's own UI language is unaffected — the leak is what pages see, not what the user reads.

## Compatibility impact
Sites auto-detecting language will offer English; the user picks their language on the site or turns the protection off for it. This is the most user-visible level-1 protection, so the setting is surfaced prominently in the shields panel with a per-site override.

## Performance impact
None.

## Test cases
- `navigator.languages` has exactly one entry, `en-US`.
- Header and JS values agree (a mismatch is itself a fingerprint).
- Browser UI language unchanged.
- Per-site override restores the real locale.
