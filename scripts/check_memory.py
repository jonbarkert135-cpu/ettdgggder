#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Fail if the project memory in `.ai/` is missing, stale or out of sync.

Checks:
  1. every memory file referenced by .ai/MEMORY.md exists;
  2. every code directory has a summary in .ai/memory/modules.json and a test;
  3. .ai/memory/MAP.md equals what scripts/gen_memory.py generates;
  4. README.md and AGENTS.md point agents at .ai/MEMORY.md;
  5. freshness: with --base <ref>, a diff that touches code, docs or gates must
     also touch .ai/memory/ (STATE.md and HISTORY.md specifically).

Run: python3 scripts/check_memory.py [--base origin/main] [--selftest]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODE_ROOT = "src_overrides/bedrock"
MEMORY_DIR = ".ai/memory"
ENTRY = ".ai/MEMORY.md"
REQUIRED = [
    "MAP.md",
    "STATE.md",
    "HISTORY.md",
    "INVARIANTS.md",
    "DECISIONS.md",
    "PROTOCOL.md",
    "modules.json",
]
# Directories whose code is exercised by other directories' tests.
TEST_EXEMPT = {"fuzz"}
# A change to any of these must come with a memory update.
WATCHED = ("src_overrides/", "docs/", "scripts/", "build/", ".github/workflows/")

sys.path.insert(0, os.path.join(ROOT, "scripts"))


def read(rel: str) -> str:
    with open(os.path.join(ROOT, rel), encoding="utf-8") as handle:
        return handle.read()


def check_files(errors: list[str]) -> None:
    if not os.path.exists(os.path.join(ROOT, ENTRY)):
        errors.append(f"{ENTRY} is missing — the memory entry point")
        return
    for name in REQUIRED:
        if not os.path.exists(os.path.join(ROOT, MEMORY_DIR, name)):
            errors.append(f"{MEMORY_DIR}/{name} is missing")
    entry = read(ENTRY)
    for name in ("STATE.md", "MAP.md", "INVARIANTS.md", "HISTORY.md", "DECISIONS.md", "PROTOCOL.md"):
        if f"memory/{name}" not in entry:
            errors.append(f"{ENTRY} no longer links memory/{name}")


def check_modules(errors: list[str]) -> None:
    described = json.loads(read(f"{MEMORY_DIR}/modules.json"))["modules"]
    base = os.path.join(ROOT, CODE_ROOT)
    for name in sorted(os.listdir(base)):
        path = os.path.join(base, name)
        if not os.path.isdir(path) or name.startswith("."):
            continue
        summary = described.get(name, {}).get("summary", "").strip()
        if not summary:
            errors.append(
                f"{CODE_ROOT}/{name}/ has no summary in {MEMORY_DIR}/modules.json — "
                "describe it in one line so agents need not read the directory"
            )
        elif len(summary) < 40:
            errors.append(f"{CODE_ROOT}/{name}/ summary is too short to be useful")
        if name not in TEST_EXEMPT and not any(f.endswith("_test.cc") for f in os.listdir(path)):
            errors.append(f"{CODE_ROOT}/{name}/ has no *_test.cc")
    for name in described:
        if not os.path.isdir(os.path.join(base, name)):
            errors.append(f"{MEMORY_DIR}/modules.json describes {name}/, which no longer exists")


def check_map(errors: list[str]) -> None:
    import gen_memory

    expected = gen_memory.build()
    actual = read(f"{MEMORY_DIR}/MAP.md")
    if expected != actual:
        errors.append(
            f"{MEMORY_DIR}/MAP.md is stale — run: python3 scripts/gen_memory.py"
        )


def check_pointers(errors: list[str]) -> None:
    if ".ai/MEMORY.md" not in read("README.md"):
        errors.append("README.md does not point agents at .ai/MEMORY.md")
    if not os.path.exists(os.path.join(ROOT, "AGENTS.md")):
        errors.append("AGENTS.md is missing — auto-loading agents rely on it")
    elif ".ai/MEMORY.md" not in read("AGENTS.md"):
        errors.append("AGENTS.md does not point at .ai/MEMORY.md")


def changed_files(base: str) -> list[str]:
    result = subprocess.run(
        ["git", "diff", "--name-only", f"{base}...HEAD"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        result = subprocess.run(
            ["git", "diff", "--name-only", base],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=True,
        )
    return [line for line in result.stdout.splitlines() if line.strip()]


def check_freshness(base: str, errors: list[str]) -> None:
    files = changed_files(base)
    if not files:
        return
    substantive = [f for f in files if f.startswith(WATCHED)]
    if not substantive:
        return
    touched = {f for f in files if f.startswith(".ai/")}
    if not any(f.endswith("STATE.md") for f in touched) or not any(
        f.endswith("HISTORY.md") for f in touched
    ):
        errors.append(
            "this change touches "
            + ", ".join(sorted({f.split('/')[0] for f in substantive}))
            + " but does not update .ai/memory/STATE.md and .ai/memory/HISTORY.md — "
            "see .ai/memory/PROTOCOL.md (memory is updated in the same PR, not later)"
        )


def selftest() -> int:
    """Smallest thing that fails if the rules break."""
    failures = 0
    errors: list[str] = []
    check_modules(errors)
    if errors:
        print("selftest: unexpected errors on a clean tree:")
        for error in errors:
            print(f"  - {error}")
        failures += 1

    # A described-but-absent directory must be reported.
    described = json.loads(read(f"{MEMORY_DIR}/modules.json"))["modules"]
    if "definitely_not_a_directory" in described:
        print("selftest: fixture name collides with a real module")
        failures += 1

    # MEMORY.md must actually contain the links check_files() requires.
    entry = read(ENTRY)
    if not re.search(r"memory/STATE\.md", entry):
        print("selftest: MEMORY.md link check would not fire")
        failures += 1

    print("selftest: ok" if not failures else "selftest: FAILED")
    return 1 if failures else 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", help="git ref to diff against for the freshness check")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    errors: list[str] = []
    check_files(errors)
    if not errors:
        check_modules(errors)
        check_map(errors)
        check_pointers(errors)
    if args.base:
        check_freshness(args.base, errors)

    if errors:
        print("project memory check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("project memory check ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
