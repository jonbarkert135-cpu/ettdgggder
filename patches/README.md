# Patches

Applied by `build/sync.py` in sorted path order against the pinned Chromium tree.

- `patches/bedrock/` — patches authored here. MPL-2.0.
- `patches/upstream/<project>/` — patches adopted from an upstream project. The header of
  each file MUST record: upstream project, upstream path, upstream revision, license.
  A patch may only be added here if the project has a row in `docs/THIRD_PARTY.md` with a
  reuse mode of `port` or `patched-base`.

Prefer a file in `src_overrides/` (mirrors the Chromium tree layout, symlinked in) over a
patch when you are adding a new file rather than changing an existing one — new files do not
conflict on Chromium rolls, patches do.

Full rules, required header fields and the audit commands: [../docs/PATCHES.md](../docs/PATCHES.md).
