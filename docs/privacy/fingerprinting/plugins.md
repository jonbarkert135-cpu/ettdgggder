# Plugins and MIME types

**Surface id:** `plugins` · **Levels:** 0 allow · 1–2 normalize · 3 block

## Attack vector
`navigator.plugins` and `navigator.mimeTypes` historically leaked installed software. Modern browsers ship a hard-coded PDF viewer list, but ordering and presence still differ between builds.

## Mitigation
Level 1+ returns exactly the spec-mandated PDF viewer entries in a fixed order. Level 3 returns empty lists.

## Compatibility impact
None in practice; PDF handling is unaffected.

## Performance impact
None.

## Test cases
- `navigator.plugins.length` is identical across installs.
- Entry order is fixed.
