#!/usr/bin/env python3
"""Fail if the security testing setup has decayed. Run:
python3 scripts/check_security_testing.py

Roadmap item 43 lists the kinds of testing that must exist and the subsystems
that must be covered. The usual way a project loses them is not deletion — it is
a subsystem added later that nobody wired into the suite, or a fuzz harness that
stopped compiling. Both are checkable.
"""

from __future__ import annotations

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SOURCES = REPO / "src_overrides" / "bedrock"

# Item 43: "test especially these". Each maps to the directory that must carry
# both implementation and tests.
CRITICAL_AREAS = {
    "networking": "net",
    "URL parser": "omnibox",
    "extension system": "extensions",
    "content blocking": "blocking",
    "privacy APIs": "privacy",
    "storage isolation": "net",
    "permissions": "extensions",
    "renderer-facing policy": "privacy",
    "downloads": "downloads",
    "passwords": "passwords",
    "update path": "update",
}

# Sanitizer and fuzzing configurations that must stay in the tree.
REQUIRED_BUILD_ARGS = {
    "build/args/bedrock-asan.gn": ["is_asan", "is_ubsan"],
    "build/args/bedrock-msan.gn": ["is_msan"],
    "build/args/bedrock-tsan.gn": ["is_tsan"],
    "build/args/bedrock-fuzz.gn": ["use_libfuzzer"],
}

REQUIRED_DOCS = {
    "docs/security/TESTING.md": ["static analysis", "sanitizers", "fuzzing",
                                 "regression", "network privacy"],
    "docs/security/THREAT_MODEL.md": ["malicious website", "malicious ad",
                                      "renderer exploit", "network observer",
                                      "compromised extension", "out of scope"],
}

MIN_FUZZERS = 4


def main() -> int:
    errors: list[str] = []

    for area, directory in CRITICAL_AREAS.items():
        path = SOURCES / directory
        if not path.is_dir():
            errors.append(f"{area}: no code at src_overrides/bedrock/{directory}")
            continue
        tests = list(path.glob("*_test.cc"))
        if not tests:
            errors.append(f"{area}: src_overrides/bedrock/{directory} has no test")

    # Every component directory needs a test, not just the listed ones — this is
    # what catches the subsystem added next month.
    for directory in sorted(p for p in SOURCES.iterdir() if p.is_dir()):
        if directory.name == "fuzz":
            continue
        sources = [p for p in directory.glob("*.cc") if not p.name.endswith("_test.cc")]
        if sources and not list(directory.glob("*_test.cc")):
            errors.append(
                f"src_overrides/bedrock/{directory.name}/ has code but no *_test.cc")

    fuzzers = sorted((SOURCES / "fuzz").glob("*_fuzzer.cc"))
    if len(fuzzers) < MIN_FUZZERS:
        errors.append(
            f"only {len(fuzzers)} fuzz harnesses; item 43 expects at least {MIN_FUZZERS}")
    for fuzzer in fuzzers:
        text = fuzzer.read_text()
        if "LLVMFuzzerTestOneInput" not in text:
            errors.append(f"{fuzzer.name}: not a libFuzzer harness")

    for name, needles in REQUIRED_BUILD_ARGS.items():
        path = REPO / name
        if not path.is_file():
            errors.append(f"{name} is missing")
            continue
        text = path.read_text()
        for needle in needles:
            if needle not in text:
                errors.append(f"{name}: expected {needle}")

    for name, needles in REQUIRED_DOCS.items():
        path = REPO / name
        if not path.is_file():
            errors.append(f"{name} is missing")
            continue
        text = path.read_text().lower()
        for needle in needles:
            if needle.lower() not in text:
                errors.append(f"{name}: expected to cover {needle!r}")

    if errors:
        print("security testing check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(f"security testing OK: {len(CRITICAL_AREAS)} critical areas tested, "
          f"{len(fuzzers)} fuzz harnesses, {len(REQUIRED_BUILD_ARGS)} sanitizer configs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
