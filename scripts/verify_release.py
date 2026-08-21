#!/usr/bin/env python3
"""Verify a downloaded Bedrock release (roadmap item 70).

    python3 scripts/verify_release.py --manifest manifest.json \
                                      --artifact bedrock-1.0.0.1-linux-x64.tar.xz
    python3 scripts/verify_release.py --manifest manifest.json --keys KEYS \
                                      --signature manifest.json.sig
    python3 scripts/verify_release.py --selftest

Standard library only, works offline, and safe to run before trusting anything
in this repository — the point of a verification tool is that the person running
it does not have to trust the project that wrote it.

What it checks:

  1. the manifest carries every field a release must state (docs/RELEASES.md);
  2. the artifact's SHA-256 matches the digest in the manifest;
  3. the manifest's signature verifies against a key list, if one is given
     (delegated to `ssh-keygen -Y verify`: OpenSSH is already on the machine and
     re-implementing signature verification in a verification script is how you
     get a verification script with a bug in it);
  4. the provenance attestation, if present, is about this artifact and this
     Chromium base.

Every failure is loud and specific. There is no "probably fine" exit path.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys

REQUIRED_MANIFEST_FIELDS = [
    "version",
    "channel",
    "chromium_version",
    "chromium_commit",
    "overlay_commit",
    "artifacts",
]
REQUIRED_ARTIFACT_FIELDS = ["name", "sha256", "size"]
SIGNATURE_NAMESPACE = "bedrock-release"


def sha256_of(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def check_manifest(manifest: dict) -> list[str]:
    problems = [
        f"manifest: missing field '{field}'"
        for field in REQUIRED_MANIFEST_FIELDS
        if not manifest.get(field)
    ]
    for index, artifact in enumerate(manifest.get("artifacts") or []):
        for field in REQUIRED_ARTIFACT_FIELDS:
            if not artifact.get(field):
                problems.append(f"manifest: artifact {index} missing '{field}'")
        digest = artifact.get("sha256", "")
        if digest and (len(digest) != 64 or not all(c in "0123456789abcdef" for c in digest)):
            problems.append(f"manifest: artifact {artifact.get('name')} has a malformed sha256")
    return problems


def check_artifact(manifest: dict, artifact_path: pathlib.Path) -> list[str]:
    entry = next(
        (a for a in manifest.get("artifacts", []) if a.get("name") == artifact_path.name), None
    )
    if entry is None:
        return [
            f"{artifact_path.name} is not listed in the manifest — you are holding a file the "
            f"release does not describe"
        ]
    actual = sha256_of(artifact_path)
    if actual != entry["sha256"]:
        return [
            f"{artifact_path.name}: DIGEST MISMATCH\n"
            f"    manifest: {entry['sha256']}\n"
            f"    file:     {actual}"
        ]
    return []


def check_signature(
    manifest_path: pathlib.Path, signature: pathlib.Path, keys: pathlib.Path, identity: str
) -> list[str]:
    if not shutil.which("ssh-keygen"):
        return ["ssh-keygen is not installed, so the signature cannot be verified here"]
    result = subprocess.run(
        [
            "ssh-keygen", "-Y", "verify",
            "-f", str(keys),
            "-I", identity,
            "-n", SIGNATURE_NAMESPACE,
            "-s", str(signature),
        ],
        stdin=manifest_path.open("rb"),
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return [f"signature verification FAILED: {result.stderr.strip() or result.stdout.strip()}"]
    return []


def check_provenance(manifest: dict, provenance: pathlib.Path) -> list[str]:
    data = json.loads(provenance.read_text(encoding="utf-8"))
    problems = []
    subjects = {s.get("name") for s in data.get("subject", [])}
    for artifact in manifest.get("artifacts", []):
        if artifact["name"] not in subjects:
            problems.append(f"provenance does not cover {artifact['name']}")
    parameters = (
        data.get("predicate", {}).get("buildDefinition", {}).get("externalParameters", {})
    )
    for field, manifest_key in (
        ("chromiumVersion", "chromium_version"),
        ("overlayCommit", "overlay_commit"),
    ):
        if parameters.get(field) != manifest.get(manifest_key):
            problems.append(
                f"provenance {field}={parameters.get(field)!r} does not match manifest "
                f"{manifest_key}={manifest.get(manifest_key)!r}"
            )
    return problems


def selftest() -> int:
    import tempfile

    with tempfile.TemporaryDirectory() as directory:
        root = pathlib.Path(directory)
        artifact = root / "bedrock-1.0.0.1-linux-x64.tar.xz"
        artifact.write_bytes(b"not really a browser")
        manifest = {
            "version": "1.0.0.1",
            "channel": "stable",
            "chromium_version": "151.0.7922.173",
            "chromium_commit": "a" * 40,
            "overlay_commit": "b" * 40,
            "artifacts": [
                {"name": artifact.name, "sha256": sha256_of(artifact), "size": artifact.stat().st_size}
            ],
        }
        assert check_manifest(manifest) == []
        assert check_artifact(manifest, artifact) == []

        manifest["artifacts"][0]["sha256"] = "0" * 64
        assert "DIGEST MISMATCH" in check_artifact(manifest, artifact)[0]

        del manifest["channel"]
        assert any("channel" in problem for problem in check_manifest(manifest))

        provenance = root / "p.json"
        provenance.write_text(json.dumps({
            "subject": [{"name": artifact.name}],
            "predicate": {"buildDefinition": {"externalParameters": {
                "chromiumVersion": "150.0.0.1", "overlayCommit": "b" * 40}}},
        }))
        manifest["channel"] = "stable"
        assert any("chromiumVersion" in p for p in check_provenance(manifest, provenance))
    print("selftest OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=pathlib.Path)
    parser.add_argument("--artifact", type=pathlib.Path, action="append", default=[])
    parser.add_argument("--signature", type=pathlib.Path)
    parser.add_argument("--keys", type=pathlib.Path)
    parser.add_argument("--identity", default="release@bedrock")
    parser.add_argument("--provenance", type=pathlib.Path)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if not args.manifest:
        parser.error("--manifest is required")

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    problems = check_manifest(manifest)
    checked = []

    if args.signature or args.keys:
        if not (args.signature and args.keys):
            parser.error("--signature and --keys go together")
        problems += check_signature(args.manifest, args.signature, args.keys, args.identity)
        checked.append("signature")

    for artifact in args.artifact:
        problems += check_artifact(manifest, artifact)
        checked.append(artifact.name)

    if args.provenance:
        problems += check_provenance(manifest, args.provenance)
        checked.append("provenance")

    if problems:
        print("VERIFICATION FAILED:", file=sys.stderr)
        for problem in problems:
            print(f"  - {problem}", file=sys.stderr)
        return 1

    if not args.artifact and not args.signature:
        print(
            "manifest is well formed, but nothing was verified against it — pass --artifact "
            "and, if you have the keys, --signature/--keys",
            file=sys.stderr,
        )
        return 1
    print(f"verified: {', '.join(checked)} — release {manifest['version']} ({manifest['channel']})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
