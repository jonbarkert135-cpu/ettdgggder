#!/usr/bin/env python3
"""Package a local component build into a release archive and its manifest.

    python3 scripts/package_release.py --version 0.0.2-dev
    BEDROCK_OUT=/path/to/out/Release python3 scripts/package_release.py \
        --version 0.0.2-dev --outdir /tmp/dist

Why a script and not a documented sequence of commands: the first release was
assembled by hand, and a hand-assembled archive is one forgotten `.so` away from
an artifact that starts on the build machine and nowhere else. The file list here
is *derived* — every shared library the binaries actually load, found by walking
`ldd` transitively and keeping only libraries that live in the output directory —
so a build that grows a dependency ships it without anyone remembering to.

It writes the layout the release notes describe:

    bedrock-<version>-linux-x64/
        chrome, chrome_crashpad_handler, *.so, *.pak, icudtl.dat, snapshots,
        locales/, MEIPreload/, resources/, angledata/ …
        run-bedrock.sh          component build: needs LD_LIBRARY_PATH
        LICENSE, THIRD_PARTY_NOTICES, RELEASE_NOTES.md

and next to it `manifest.json` in the shape `scripts/verify_release.py` checks,
so the digest a user verifies is produced by the same run that produced the file.

Standard library only. Does not build, does not publish, does not sign.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
OUT_DIR = pathlib.Path(os.environ.get("BEDROCK_OUT", "/work/chromium/src/out/Release"))

# Data files and directories a Chromium build needs at runtime. Missing entries
# are reported, not silently skipped: a release with no resources.pak would
# still tar up cleanly.
DATA_FILES = [
    "chrome_100_percent.pak",
    "chrome_200_percent.pak",
    "resources.pak",
    "icudtl.dat",
    "snapshot_blob.bin",
    "v8_context_snapshot.bin",
]
DATA_DIRS = ["locales", "resources", "MEIPreload", "angledata"]
OPTIONAL_DIRS = ["PrivacySandboxAttestationsPreloaded", "IwaKeyDistribution", "hyphen-data"]
BINARIES = ["chrome", "chrome_crashpad_handler"]

RUN_SCRIPT = """#!/bin/sh
# Bedrock is built as a Chromium *component* build: the browser is split across
# several hundred shared libraries that sit next to this script, and the dynamic
# loader has to be told where they are. That is the only thing this wrapper does.
#
# There is no installer and no desktop entry yet. Use a throwaway profile
# directory; this is a developer build, not a maintained one.
here=$(cd "$(dirname "$0")" && pwd)
LD_LIBRARY_PATH="$here${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \\
  exec "$here/chrome" --user-data-dir="${BEDROCK_PROFILE:-$here/profile}" "$@"
"""


def local_libraries(out: pathlib.Path) -> list[str]:
    """Every shared library in `out` reachable from the shipped binaries."""
    seen: set[str] = set()
    queue = list(BINARIES)
    while queue:
        name = queue.pop()
        if name in seen:
            continue
        seen.add(name)
        target = out / name
        if not target.exists():
            continue
        result = subprocess.run(
            ["ldd", str(target)], capture_output=True, text=True, timeout=120
        )
        for line in result.stdout.splitlines():
            match = re.search(r"=> (\S+)", line)
            if match and match.group(1).startswith(str(out)):
                queue.append(pathlib.Path(match.group(1)).name)
    return sorted(name for name in seen if name.endswith(".so"))


def sha256_of(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git(*args: str) -> str:
    return subprocess.run(
        ["git", "-C", str(REPO), *args], capture_output=True, text=True
    ).stdout.strip()


def chromium_pin() -> tuple[str, str]:
    text = (REPO / "build" / "chromium.pin").read_text()
    version = re.search(r"(?m)^version=(\S+)", text)
    commit = re.search(r"(?m)^commit=(\S+)", text)
    if not version or not commit:
        sys.exit("build/chromium.pin: expected 'version=' and 'commit=' lines")
    return version.group(1), commit.group(1)


def stage(out: pathlib.Path, target: pathlib.Path, version: str) -> None:
    if target.exists():
        shutil.rmtree(target)
    target.mkdir(parents=True)

    missing: list[str] = []
    for name in BINARIES + DATA_FILES:
        source = out / name
        if not source.exists():
            missing.append(name)
            continue
        shutil.copy2(source, target / name)
        (target / name).chmod(0o755 if name in BINARIES else 0o644)

    libraries = local_libraries(out)
    for name in libraries:
        source = out / name
        if source.exists():
            shutil.copy2(source, target / name)
        else:
            missing.append(name)

    for name in DATA_DIRS:
        source = out / name
        if not source.is_dir():
            missing.append(name + "/")
            continue
        shutil.copytree(source, target / name)
    for name in OPTIONAL_DIRS:
        source = out / name
        if source.is_dir():
            shutil.copytree(source, target / name)

    if missing:
        sys.exit("refusing to package, missing from the build: " + ", ".join(missing))

    run = target / "run-bedrock.sh"
    run.write_text(RUN_SCRIPT)
    run.chmod(0o755)

    shutil.copy2(REPO / "LICENSE", target / "LICENSE")
    # THIRD_PARTY_NOTICES is a directory of per-project notice files, and the
    # licences require shipping all of them, not a summary.
    shutil.copytree(REPO / "THIRD_PARTY_NOTICES", target / "THIRD_PARTY_NOTICES")
    notes = REPO / "docs" / "releases" / f"{version}.md"
    if notes.exists():
        shutil.copy2(notes, target / "RELEASE_NOTES.md")
    else:
        print(f"note: docs/releases/{version}.md does not exist yet", file=sys.stderr)

    print(f"staged {len(libraries)} libraries + {len(DATA_FILES)} data files in {target}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True, help="e.g. 0.0.2-dev")
    parser.add_argument("--channel", default="nightly")
    parser.add_argument("--outdir", default="/work/dist")
    parser.add_argument("--skip-archive", action="store_true",
                        help="stage the directory only")
    args = parser.parse_args()

    if not (OUT_DIR / "chrome").exists():
        sys.exit(f"no build at {OUT_DIR} (see build/LOCAL_BUILD_HANDOFF.md)")

    name = f"bedrock-{args.version}-linux-x64"
    dist = pathlib.Path(args.outdir)
    dist.mkdir(parents=True, exist_ok=True)
    stage(OUT_DIR, dist / name, args.version)
    if args.skip_archive:
        return 0

    archive = dist / f"{name}.tar.zst"
    if archive.exists():
        archive.unlink()
    subprocess.run(
        ["tar", "--use-compress-program=zstd -19 -T0", "-cf", str(archive), name],
        cwd=dist, check=True,
    )

    chromium_version, chromium_commit = chromium_pin()
    manifest = {
        "version": args.version,
        "channel": args.channel,
        "chromium_version": chromium_version,
        "chromium_commit": chromium_commit,
        "overlay_commit": git("rev-parse", "HEAD"),
        "artifacts": [{
            "name": archive.name,
            "sha256": sha256_of(archive),
            "size": archive.stat().st_size,
        }],
    }
    (dist / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    print(json.dumps(manifest, indent=2))
    print(f"\narchive: {archive} ({archive.stat().st_size} bytes)")
    print("verify with: python3 scripts/verify_release.py "
          f"--manifest {dist / 'manifest.json'} --artifact {archive}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
