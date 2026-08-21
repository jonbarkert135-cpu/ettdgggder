#!/usr/bin/env python3
"""Upstream sync tooling (roadmap item 68).

Four jobs, all offline except the one that needs a Chromium tree:

    python3 scripts/upstream_sync.py --status                 pin age, is a roll due
    python3 scripts/upstream_sync.py --check-patches          headers, fields, stale versions
    python3 scripts/upstream_sync.py --dry-run -w ~/bedrock   git apply --check, conflicts only
    python3 scripts/upstream_sync.py --plan                   the pipeline, with commands
    python3 scripts/upstream_sync.py --selftest

Deliberately thin. depot_tools fetches, build/sync.py overlays, run_host_tests.sh tests; this
script only answers the questions a person asks before starting a roll, and it answers them
without a 100 GB checkout.
"""

from __future__ import annotations

import argparse
import datetime
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
PIN = REPO / "build" / "chromium.pin"
PATCHES = REPO / "patches"

REQUIRED_FIELDS = [
    "Bedrock-Patch",
    "Area",
    "Upstream-Paths",
    "Reason",
    "Owner",
    "Chromium-Version",
    "Drop-When",
]

# Paths where upstream churn is heavy enough that a patch there should expect to
# be rewritten rather than re-applied. Used only to warn before a roll.
VOLATILE_PREFIXES = (
    "third_party/blink/",
    "content/browser/",
    "net/",
    "services/network/",
    "chrome/browser/ui/",
)

STAGES = [
    ("security review", "read the upstream security notes; check them against patches/"),
    ("privacy patches", "python3 build/sync.py --workspace WS --overlay-only"),
    ("browser patches", "python3 scripts/upstream_sync.py --dry-run --workspace WS"),
    ("automated tests", "./scripts/run_host_tests.sh && autoninja -C out/Release chrome"),
    ("privacy regression tests", "see docs/security/TESTING.md"),
]


def read_pin() -> dict[str, str]:
    pin: dict[str, str] = {}
    for line in PIN.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("#"):
            match = re.search(r"on (\d{4}-\d{2}-\d{2})", line)
            if match:
                pin["verified"] = match.group(1)
            continue
        if line:
            key, _, value = line.partition("=")
            pin[key.strip()] = value.strip()
    return pin


def parse_header(text: str) -> dict[str, str]:
    header: dict[str, str] = {}
    for line in text.splitlines():
        if not line.startswith("#"):
            break
        match = re.match(r"#\s*([\w-]+):\s*(.*)", line)
        if match:
            header[match.group(1)] = match.group(2).strip()
    return header


def patch_files() -> list[pathlib.Path]:
    return sorted(PATCHES.rglob("*.patch")) if PATCHES.exists() else []


def check_patches(pin: dict[str, str]) -> list[str]:
    problems: list[str] = []
    for path in patch_files():
        header = parse_header(path.read_text(encoding="utf-8"))
        relative = path.relative_to(REPO)
        for field in REQUIRED_FIELDS:
            if not header.get(field):
                problems.append(f"{relative}: missing header field {field}")
        version = header.get("Chromium-Version", "")
        if version and version != pin.get("version"):
            problems.append(
                f"{relative}: last verified against Chromium {version}, tree is pinned to "
                f"{pin.get('version')} — re-verify or update the header"
            )
        if path.parts[path.parts.index("patches") + 1] == "upstream":
            for field in ("Upstream-Project", "Upstream-Revision", "License"):
                if not header.get(field):
                    problems.append(f"{relative}: adopted patch missing {field}")
    return problems


def volatile_warnings() -> list[str]:
    warnings: list[str] = []
    for path in patch_files():
        header = parse_header(path.read_text(encoding="utf-8"))
        for upstream_path in header.get("Upstream-Paths", "").split(","):
            upstream_path = upstream_path.strip()
            if upstream_path.startswith(VOLATILE_PREFIXES):
                warnings.append(
                    f"{path.relative_to(REPO)}: patches {upstream_path} — high-churn area, "
                    f"expect to rewrite rather than re-apply"
                )
    return warnings


def status(pin: dict[str, str]) -> int:
    verified = pin.get("verified")
    print(f"pinned Chromium: {pin.get('version')} ({pin.get('channel', '?')}, "
          f"milestone {pin.get('milestone', '?')})")
    print(f"commit:          {pin.get('commit')}")
    print(f"patches:         {len(patch_files())}")
    if not verified:
        print("pin age:         unknown — chromium.pin has no 'verified ... on YYYY-MM-DD' comment")
        return 1
    age = (datetime.date.today() - datetime.date.fromisoformat(verified)).days
    print(f"pin verified:    {verified} ({age} days ago)")
    # Two weeks is the cadence from docs/UPSTREAM_SYNC.md: small rolls stay cheap.
    print("roll due:        " + ("YES — see docs/UPSTREAM_SYNC.md" if age > 14 else "no"))
    return 0


def dry_run(workspace: pathlib.Path) -> int:
    src = workspace / "src"
    if not src.exists():
        sys.exit(f"{src} does not exist — run build/sync.py --workspace {workspace} first")
    failed = []
    for path in patch_files():
        result = subprocess.run(
            ["git", "apply", "--check", str(path)], cwd=src, capture_output=True, text=True
        )
        state = "ok" if result.returncode == 0 else "CONFLICT"
        print(f"{state:9} {path.relative_to(REPO)}")
        if result.returncode != 0:
            failed.append((path, result.stderr.strip()))
    for path, error in failed:
        print(f"\n--- {path.relative_to(REPO)}\n{error}", file=sys.stderr)
    print(f"\n{len(patch_files()) - len(failed)} applied cleanly, {len(failed)} conflict")
    return 1 if failed else 0


def plan() -> int:
    print("Upstream roll — docs/UPSTREAM_SYNC.md\n")
    for number, (stage, command) in enumerate(STAGES, 1):
        print(f"{number}. {stage}\n     {command}")
    print("\n6. release candidate\n     evaluate with bedrock::update::Evaluate "
          "(src_overrides/bedrock/updater/release_policy.h)")
    return 0


def selftest() -> int:
    header = parse_header(
        "# Bedrock-Patch: 0001-x\n# Area: privacy\n# Reason: item 12\n"
        "not a comment: ignored\n# Owner: nobody\n"
    )
    assert header["Area"] == "privacy", header
    assert "Owner" not in header, "parsing must stop at the first non-comment line"
    pin = read_pin()
    assert pin.get("version"), "chromium.pin has no version"
    assert re.fullmatch(r"[0-9a-f]{40}", pin.get("commit", "")), "pin commit is not a full sha"
    assert len(STAGES) == 5, "the pipeline in docs/UPSTREAM_SYNC.md has five stages"
    print("selftest OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--status", action="store_true")
    parser.add_argument("--check-patches", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--plan", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("-w", "--workspace", type=pathlib.Path)
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if args.plan:
        return plan()
    if args.dry_run:
        if not args.workspace:
            sys.exit("--dry-run needs --workspace pointing at a Chromium checkout")
        return dry_run(args.workspace)

    pin = read_pin()
    if args.check_patches:
        problems = check_patches(pin)
        for warning in volatile_warnings():
            print(f"warning: {warning}")
        for problem in problems:
            print(f"error: {problem}", file=sys.stderr)
        print(f"{len(patch_files())} patch(es) checked, {len(problems)} problem(s)")
        return 1 if problems else 0
    return status(pin)


if __name__ == "__main__":
    sys.exit(main())
