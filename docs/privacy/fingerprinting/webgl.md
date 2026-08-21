# WebGL

**Surface id:** `webgl` · **Levels:** 0 allow · 1–2 farble · 3 block

## Attack vector
`WEBGL_debug_renderer_info` exposes the exact GPU and driver string (`ANGLE (NVIDIA GeForce RTX 4070 …)`), which is near-unique in combination with screen size. Shader-precision formats, supported extensions and `readPixels()` output add more stable entropy.

## Mitigation
Renderer and vendor strings are normalized to a generic pair (a common software/ANGLE profile) from level 1. Parameter queries are clamped to widely-shared values, and the extension list is reduced to a common core set. `readPixels()` is perturbed with the same deterministic scheme as canvas, seeded with `kWebgl`.

## Compatibility impact
Level 1: sites that branch on GPU strings pick a conservative path — usually fine, occasionally a lower-quality renderer. Level 2 flagged as site-breaking: some WebGL games and 3D viewers refuse to start. Level 3 disables WebGL entirely.

## Performance impact
String and parameter queries are cheap and rare. `readPixels()` cost matches canvas.

## Test cases
- `UNMASKED_RENDERER_WEBGL` identical across sites at level 1.
- Extension list identical across two machines with different GPUs.
- `readPixels()` deterministic per (site, session).
- A basic three.js scene still renders at level 1.
