# Supply chain security

**Roadmap item 70.** A browser is the most attractive supply-chain target a desktop has: it is
installed on every machine, it updates itself, and it runs with the user's full privileges. An
attacker who reaches the build or the update path does not need a single browser vulnerability.
That is not theoretical — it is how several widely used desktop applications have been backdoored,
and in every case the compromise was in a step nobody was watching, not in the code review.

This document is the chain from an upstream source archive to the binary a user runs, one link at
a time, with the honest status of each. Items 39–42 already built parts of it; this is where they
are tied together and where the missing links are named.

## The chain

| Link | Mechanism | Where | Status |
| --- | --- | --- | --- |
| Dependency lock | pinned version per component, no ranges, no `main` | [`docs/THIRD_PARTY.md`](THIRD_PARTY.md) | enforced by `check_provenance.py` |
| Chromium lock | version + 40-char commit | [`build/chromium.pin`](../build/chromium.pin) | enforced |
| Archive hashes | SHA-256 per fetched artifact | [`build/dependency-hashes.txt`](../build/dependency-hashes.txt) | partial — only verified digests are listed |
| SBOM | CycloneDX 1.5, generated from the inventory | [`build/sbom.json`](../build/sbom.json) | generated, `--check` in CI |
| Trusted source verification | fetch over HTTPS from the project's own origin, verify the commit or the digest before use | `build/sync.py` | commit-verified for Chromium; per-archive verification grows with the hash file |
| Build provenance | in-toto/SLSA-style attestation per artifact | `release/provenance-<version>.json` | **format defined below, not yet produced — no release exists** |
| Release signatures | detached signature over the manifest and each artifact | release keys, below | **format defined, keys not yet generated** |
| Update path | signed manifest, hash-checked payload, no downgrade | `updater/update_provider.*` | code + tests, item 40 |

Two of those rows say "not yet". They stay that way in writing until a release actually exists;
a supply-chain document that describes signatures nobody produces is the exact false assurance
item 55 forbids in the UI, and it is no better in a document.

## Dependency lock and hashes

Every third-party component has one row in the inventory with an exact version. `main`, `latest`
and an empty version are refused by the gate. The only exception is reuse mode `reimplement`,
which ships no upstream code at all — there is nothing to pin.

`build/dependency-hashes.txt` holds `sha256  <Project>` lines for artifacts that were fetched and
checked. **A component with no verified digest has no row**, rather than a placeholder: the SBOM is
generated from this file, and an SBOM with invented digests is worse than an incomplete one,
because it will be believed.

Chromium is not in that file. It is fetched by commit, and a git commit hash covers the tree more
strongly than a digest over a tarball someone rolled.

## Trusted sources

A source is trusted when all four hold, and the four are checked in this order:

1. **Origin** — the project's own repository or release host, over HTTPS, never a mirror of
   convenience and never a package aggregator that re-hosts.
2. **Identity** — a tag or commit that the upstream project published, not a moving branch.
3. **Integrity** — the commit hash or the archive digest matches the pinned value.
4. **Licence** — the provenance row exists with a notice file (item 3, `check_provenance.py`).

An artifact that fails any of them is not fetched. There is no "download it and check later" path,
because in practice later means never and the artifact is already on the build machine.

## Build provenance

Every released artifact gets an attestation next to it, in the in-toto statement shape used by
SLSA, so existing verifiers can read it:

```json
{
  "_type": "https://in-toto.io/Statement/v1",
  "subject": [{"name": "bedrock-1.0.0.1-linux-x64.tar.xz",
               "digest": {"sha256": "…"}}],
  "predicateType": "https://slsa.dev/provenance/v1",
  "predicate": {
    "buildDefinition": {
      "buildType": "https://bedrock.invalid/build/v1",
      "externalParameters": {
        "overlayCommit": "…", "chromiumVersion": "151.0.7922.173",
        "chromiumCommit": "…", "gnArgsSha256": "…", "sourceDateEpoch": 1750000000
      }
    },
    "runDetails": {"builder": {"id": "…"}, "metadata": {"startedOn": "…"}}
  }
}
```

Those fields are the same nine values as the reproducibility manifest
([`REPRODUCIBILITY.md`](REPRODUCIBILITY.md)) — deliberately, so a second builder can compare an
attestation against their own build instead of taking ours on trust. Provenance that cannot be
independently reproduced is a signed claim about a black box.

## Release signing

| | |
| --- | --- |
| Algorithm | Ed25519 |
| Signature format | OpenSSH detached signatures (`ssh-keygen -Y sign`), namespace `bedrock-release` |
| What is signed | the release manifest, and each artifact digest listed in it |
| Where the keys live | offline hardware tokens, one per release manager; never on a build machine |
| Rotation | annually, and immediately on any suspicion; both old and new keys are listed for one release cycle |
| Distribution | `KEYS` in the repository root and in every release; also fetchable from a second, independently hosted location so a single compromised host cannot swap them |
| Revocation | a signed revocation statement published in the same places, and the key id removed from the browser's trusted set in the next release |

The browser's updater trusts a key id, not a hostname (item 40): an attacker who takes over a
download host still cannot ship an update. The corollary is that key handling is the highest-value
target in the project, which is why it is on tokens and not in a CI secret.

## Verifying a release as a user

No trust in this repository is required to check that a download is what it claims to be:

```bash
# 1. Get the manifest, its signature, the artifact and the key list
#    (all published together with every release)
# 2. Verify the manifest signature against a key you have seen before
ssh-keygen -Y verify -f KEYS -I release@bedrock -n bedrock-release \
           -s manifest.json.sig < manifest.json

# 3. Check the artifact against the manifest, and the manifest against the SBOM
python3 scripts/verify_release.py --manifest manifest.json \
                                  --artifact bedrock-1.0.0.1-linux-x64.tar.xz
```

`scripts/verify_release.py` is deliberately dependency-free and works offline. It refuses on any
mismatch and prints which check failed; it never prints "probably fine".

## What this does not cover

- **A compromised developer machine** with a signing token plugged in. Offline keys raise the cost;
  they do not eliminate it.
- **Upstream Chromium's own supply chain.** Bedrock verifies which Chromium it built; it cannot
  independently audit everything in a 100 GB tree.
- **Distribution repositories.** When a user installs from apt/dnf/pacman, that repository's
  signing applies and the OS updater owns the path (item 40 returns
  `kManagedByOperatingSystem`). Bedrock does not duplicate or override it.
- **Reproducibility as proof.** Until an independent rebuild matches, provenance says who built it,
  not that anyone else can.
