# Reproducible Builds

Goal: two people building the same Bedrock commit on the same platform get byte-identical output,
and anyone can check that a published binary came from the published source. Bedrock is not there
yet; this document says exactly how far it is, because "reproducible builds" as an aspiration on a
README is worth nothing to the person trying to verify a binary.

## What is pinned today

| Input | Where | Status |
| --- | --- | --- |
| Chromium version + commit | `build/chromium.pin` (151.0.7922.173, `a96602f3…`) | pinned, checked by `check_provenance.py` |
| Third-party inventory | `docs/THIRD_PARTY.md` | pinned versions enforced (only `reimplement` may be unpinned) |
| Dependency archive hashes | `build/dependency-hashes.txt` | partial — rows exist only for artifacts actually verified |
| GN arguments | `build/args/bedrock-release.gn` | fixed and CI-checked for the autonomy flags |
| SBOM | `build/sbom.json` (CycloneDX 1.5) | generated from the inventory, verified in CI |
| Toolchain | Chromium's bundled clang/rustc for the pinned revision | inherited from the pin |

## The build manifest

Every release records, in one file next to the artifacts:

```
bedrock-version, chromium-version, chromium-commit,
overlay-commit, gn-args-sha256, toolchain-revision,
sbom-sha256, source-date-epoch, builder-platform
```

Two builds match when those nine values match and the artifact digests match. A mismatch in the
artifact with everything else equal is a reproducibility bug, and reporting it is useful.

## Determinism measures

- `SOURCE_DATE_EPOCH` is set from the overlay commit date; no wall-clock timestamps in the build.
- No absolute paths in the output: the Chromium checkout is remapped
  (`-ffile-prefix-map`, `-fdebug-prefix-map`).
- `is_official_build = true`, PGO disabled (`chrome_pgo_phase = 0`) — a profile that is not part of
  the source makes the output unverifiable by definition.
- No embedded build host name, user name or build counter.
- Locale- and environment-independent: builds run with a fixed `LANG` and a scrubbed environment.

## Known gaps (honest list)

1. **Chromium itself is not fully reproducible** upstream on every platform. Where upstream is not
   deterministic, neither are we, and no configuration on our side fixes that.
2. **Archive hashes are incomplete.** `build/dependency-hashes.txt` deliberately contains only
   digests that were verified against a real artifact. Invented placeholders would make the SBOM
   look complete and be wrong.
3. **No published rebuild attestation yet.** There is no second independent builder, so nobody has
   yet confirmed a match. Until that exists, treat reproducibility as *designed for*, not *proven*.
4. **macOS and Windows** are not covered; the reference target is Linux x64.

## How to verify a build yourself

```bash
python3 build/sync.py --workspace ~/bedrock-src     # fetches the pinned tree
sha256sum ~/bedrock-src/src/out/Bedrock/bedrock      # compare with the release manifest
python3 scripts/generate_sbom.py --check             # SBOM matches the inventory
python3 scripts/check_provenance.py                  # inventory matches the notices
```

If the digests differ, the build manifest tells you which of the nine inputs differed. Open an
issue with both manifests; a reproducibility failure is a real bug report, not a support question.
