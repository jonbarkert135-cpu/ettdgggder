# Gamepad enumeration

**Surface id:** `gamepad` · **Levels:** 0 allow · 1 normalize · 2–3 block

## Attack vector
`navigator.getGamepads()` exposes connected controller ids (vendor/product strings) with no permission prompt, identifying niche hardware precisely.

## Mitigation
Level 1 normalizes controller ids to a standard mapping name while keeping axes and buttons functional. Level 2+ returns an empty list until the user opts in for the site.

## Compatibility impact
Level 1 is transparent for games using the standard mapping. Level 2+ breaks controller support until the user allows it; flagged.

## Performance impact
None.

## Test cases
- Gamepad id is a fixed string at level 1 while input still works.
- Empty list at level 2 before opt-in.
