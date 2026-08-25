#!/usr/bin/env python3
"""Fetch the pinned Chromium tree, overlay Bedrock, and print the build commands.

Usage:
    python3 build/sync.py --workspace ~/bedrock-src        # fetch + checkout + overlay
    python3 build/sync.py --workspace ~/bedrock-src --overlay-only

    python3 build/sync.py --workspace D:\\src\\bedrock --no-history   # smaller checkout

This is deliberately thin: depot_tools already does the hard work. We only pin the
version, apply patches/ in order, and link src_overrides/ into the tree (copied
instead of symlinked where the OS refuses symlinks, i.e. Windows without
Developer Mode).
Requires: git, python3, ~100 GB free disk, and Chromium's build deps
(src/build/install-build-deps.sh on Linux).
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEPOT_TOOLS_URL = "https://chromium.googlesource.com/chromium/tools/depot_tools.git"


def read_pin(path: Path = REPO / "build" / "chromium.pin") -> dict[str, str]:
    pin = {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            key, _, value = line.partition("=")
            pin[key.strip()] = value.strip()
    missing = {"version", "commit"} - pin.keys()
    if missing:
        sys.exit(f"chromium.pin is missing: {', '.join(sorted(missing))}")
    return pin


def run(cmd: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    print(f"$ {' '.join(cmd)}", flush=True)
    subprocess.run(cmd, cwd=cwd, env=env, check=True)


def ensure_depot_tools(workspace: Path) -> dict[str, str]:
    depot = workspace / "depot_tools"
    if not depot.exists():
        run(["git", "clone", "--depth", "1", DEPOT_TOOLS_URL, str(depot)], cwd=workspace)
    env = dict(os.environ)
    env["PATH"] = f"{depot}{os.pathsep}{env['PATH']}"
    env.setdefault("DEPOT_TOOLS_UPDATE", "1")
    return env


def fetch_chromium(workspace: Path, pin: dict[str, str], env: dict[str, str],
                   no_history: bool = False) -> Path:
    src = workspace / "src"
    if not src.exists():
        fetch = ["fetch", "--nohooks"]
        if no_history:
            # Drops the Chromium git history: tens of GB less disk, and enough to
            # build. `git log` and bisecting upstream stop working in that tree.
            fetch.append("--no-history")
        run([*fetch, "chromium"], cwd=workspace, env=env)
    run(["git", "fetch", "--tags", "origin", pin["commit"]], cwd=src, env=env)
    run(["git", "checkout", pin["commit"]], cwd=src, env=env)
    run(["gclient", "sync", "-D", "--force", "--reset"], cwd=workspace, env=env)
    run(["gclient", "runhooks"], cwd=workspace, env=env)
    return src


def place(source: Path, target: Path) -> None:
    """Symlink source at target, falling back to a copy where symlinks are refused.

    Windows only creates symlinks with Developer Mode or elevation (WinError 1314);
    a copy builds identically, it just has to be re-run after every overlay edit —
    which is what `--overlay-only` is for.
    """
    try:
        target.symlink_to(source)
    except OSError:
        shutil.copy2(source, target)


def apply_overlay(src: Path) -> None:
    """Apply patches/**/*.patch in sorted order, then place src_overrides/ in the tree."""
    patches = sorted((REPO / "patches").rglob("*.patch"))
    for patch in patches:
        # -N: already-applied patches are skipped instead of failing a re-run.
        run(["git", "apply", "--3way", "--whitespace=nowarn", str(patch)], cwd=src)
    print(f"applied {len(patches)} patch(es)")

    overrides = REPO / "src_overrides"
    linked = 0
    for source in sorted(p for p in overrides.rglob("*") if p.is_file()):
        target = src / source.relative_to(overrides)
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.is_symlink() or target.exists():
            target.unlink()
        place(source, target)
        linked += 1
    print(f"placed {linked} override file(s)")


def next_steps(workspace: Path, src: Path, args_file: str) -> str:
    """The gn/ninja commands, in the shell of the host OS."""
    out = src / "out" / "Bedrock"
    gn_args = REPO / "build" / "args" / f"{args_file}.gn"
    if os.name == "nt":
        return (
            f"  $env:PATH = \"{workspace}{os.sep}depot_tools;$env:PATH\"\n"
            f"  cd {src}\n"
            f"  gn gen {out} --args=\"$((Get-Content {gn_args} |\n"
            f"                        Where-Object {{$_ -notmatch '^#'}}) -join ' ')\"\n"
            f"  autoninja -C {out} chrome\n"
        )
    return (
        f"  export PATH={workspace}/depot_tools:$PATH\n"
        f"  gn gen {out} --args=\"$(grep -v '^#' {gn_args} | tr '\\n' ' ')\"\n"
        f"  autoninja -C {out} chrome\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", required=True, type=Path,
                        help="directory that will hold depot_tools/ and src/")
    parser.add_argument("--overlay-only", action="store_true",
                        help="skip fetch/sync, only re-apply patches and overrides")
    parser.add_argument("--no-history", action="store_true",
                        help="fetch Chromium without git history (much smaller checkout)")
    parser.add_argument("--args", default="bedrock-release",
                        help="GN args file from build/args/ (without .gn)")
    args = parser.parse_args()

    pin = read_pin()
    workspace = args.workspace.expanduser().resolve()
    workspace.mkdir(parents=True, exist_ok=True)

    if args.overlay_only:
        src = workspace / "src"
        if not src.exists():
            sys.exit(f"{src} does not exist; run without --overlay-only first")
    else:
        env = ensure_depot_tools(workspace)
        src = fetch_chromium(workspace, pin, env, no_history=args.no_history)

    apply_overlay(src)

    print(f"\nChromium {pin['version']} ready at {src}\nNext:\n" + next_steps(workspace, src, args.args))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
