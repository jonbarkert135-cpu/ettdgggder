# 010 — Anti-fingerprinting

**Roadmap items 9 and 10.** Status: policy matrix, deterministic derivation and the per-surface
docs landed (`src_overrides/bedrock/privacy/fingerprinting/fingerprint_policy.{h,cc}`, `docs/privacy/fingerprinting/`).

## The two schools, and what we actually took

**Tor Browser** minimises uniqueness: every user reports the same values (same UA, same UTC,
letterboxed window, fixed font set), and the security slider is honest that raising it breaks
sites. It works because the population is uniform.

**Brave** perturbs instead: values are altered per session and per site so they cannot be
linked, even though they stay unique-ish.

Bedrock is **normalization-first**, with perturbation used only where normalization is
impossible without destroying the feature:

| | Approach | Surfaces |
|---|---|---|
| Normalize (Tor school) | one value for the whole population | UA, client hints, language, timezone, screen, fonts, cores, memory, plugins, voices, WebRTC, timers |
| Perturb (Brave school) | deterministic per (session, site) | canvas, WebGL readback, audio |
| Block | API absent | sensors, battery, and level-3 removals |

Canvas, WebGL and audio cannot be normalized: their output legitimately depends on what the
page drew, so a "standard value" does not exist. Everything else can be, and is.

## Four levels

| Level | Name | Posture |
|---|---|---|
| 0 | Compatibility | shims off (storage isolation still on — that is not negotiable) |
| 1 | Balanced | **default**: canvas/WebGL/audio perturbed, UA/hints/language/screen/fonts/cores/memory normalized, sensors and battery blocked |
| 2 | Strict | adds timezone→UTC, 100px letterboxing, tighter fonts, device-API prompts, coarser timers |
| 3 | Maximum | Tor-like: WASM/SharedArrayBuffer off, canvas/WebGL/audio blocked, ephemeral third-party storage, remote fonts off. **Openly labelled as breaking sites** |

The matrix is a table in `fingerprint_policy.cc`, one row per surface, and a test asserts
**monotonicity**: no surface may become weaker as the level rises. Level 0 must be a no-op —
also asserted, so an over-eager shim cannot silently leak into the compatibility mode.

## Why no random chaos (item 10)

Per-call randomness fails twice: a surface that never returns the same value is itself a
signal ("this user runs an anti-fingerprinting browser" — a small, identifying population),
and repeated sampling averages the noise back out to the true value.

Bedrock derives every value from

```
seed = mix(mix(session_secret ^ fnv1a(eTLD+1)) + surface)
value_i = f(seed, i)     // pure function, no clock, no RNG
```

with these consequences, each pinned by a test in `fingerprint_policy_test.cc`:

- **Stable within a session and site** → re-reading a canvas gives identical bytes; averaging
  gains the attacker nothing.
- **Different per site** → the same canvas on two sites hashes differently; no cross-site linking.
- **Different per session** → `session_secret` is 64 random bits generated at startup and never
  persisted; no cross-session linking. This is the *only* variation in the system, and it is
  the justified one.
- **Independent per surface** → learning the canvas noise reveals nothing about audio.
- Iframes are keyed on the **top-level** site, so an embedded third party cannot re-sample the
  same surface under a different origin.

## Documentation gate

Every surface has `docs/privacy/fingerprinting/<id>.md` with attack vector, mitigation,
compatibility impact, performance impact and test cases. `scripts/check_fp_docs.py` parses the
enum out of the policy table and fails CI if a surface is undocumented, a required section is
missing, or a doc describes a surface that no longer exists. A protection nobody can explain
cannot ship.

## Renderer integration (when the tree is checked out)

Shims live in Blink bindings, not in content scripts: `V8` wrappers for the affected getters,
with the level and site seed pushed into the renderer at commit time via the existing
`blink::WebRuntimeFeatures` / document policy plumbing. Rationale: a JS-injected shim can be
detected and unwrapped by the page, a binding-level shim cannot. Details go in the patch series
under `patches/bedrock/fingerprint/` once a Chromium checkout exists.
