#!/usr/bin/env python3
"""Fail if the project calls itself open source without the parts that make the
claim true. Run: python3 scripts/check_open_source.py

Roadmap item 41: "do not call the project open source if a critical component
cannot be studied or built". The checks are dull on purpose — the documents that
matter are the ones nobody notices are missing until they need them.
"""

from __future__ import annotations

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

REQUIRED = {
    "LICENSE": ["Mozilla Public License"],
    "README.md": ["Bedrock", "build"],
    "CONTRIBUTING.md": ["provenance", "gates", "telemetry"],
    "SECURITY.md": ["Reporting a vulnerability", "Scope", "Supported versions"],
    "docs/BUILD.md": ["sync.py", "gn", "Requirements"],
    "docs/LICENSING.md": ["Reuse decision ladder", "GPL"],
    "docs/THIRD_PARTY.md": ["BEGIN INVENTORY"],
    "docs/THREAT_MODEL.md": ["out of scope", "Assumptions", "Targeted"],
    "docs/REPRODUCIBILITY.md": ["build manifest", "Known gaps", "SBOM"],
    "build/chromium.pin": ["version=", "commit="],
    "build/sbom.json": ["CycloneDX", "components"],
    "build/dependency-hashes.txt": ["sha256"],
}

# Every component a user must be able to study and build. A component here with
# no source in the tree means the open-source claim is false.
CRITICAL_COMPONENTS = {
    "privacy engine": "src_overrides/bedrock/privacy",
    "content blocker": "src_overrides/bedrock/privacy/tracker_blocker",
    "network policy": "src_overrides/bedrock/privacy/network",
    "sessions and profiles": "src_overrides/bedrock/profiles",
    "passwords": "src_overrides/bedrock/passwords",
    "extensions": "src_overrides/bedrock/extensions",
    "update system": "src_overrides/bedrock/updater",
    "user interface": "src_overrides/bedrock/ui",
    "extension catalog": "src_overrides/bedrock/extensions/catalog",
    "knowledge base": "src_overrides/bedrock/settings/knowledge",
    "build configuration": "build/args",
}


def main() -> int:
    errors: list[str] = []

    for name, phrases in REQUIRED.items():
        path = REPO / name
        if not path.is_file():
            errors.append(f"{name} is missing")
            continue
        text = path.read_text().lower()
        for phrase in phrases:
            if phrase.lower() not in text:
                errors.append(f"{name}: expected to mention {phrase!r}")

    for label, relative in CRITICAL_COMPONENTS.items():
        directory = REPO / relative
        if not directory.is_dir():
            errors.append(f"{label}: no source at {relative}")
            continue
        sources = list(directory.rglob("*.cc")) + list(directory.rglob("*.gn")) + \
            list(directory.rglob("*.json"))
        if not sources:
            errors.append(f"{label}: {relative} contains no source to study")
        tests = [p for p in directory.rglob("*_test.cc")]
        if relative.startswith("src_overrides") and not tests:
            errors.append(f"{label}: {relative} has no test — unverifiable code")

    if errors:
        print("open-source check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(f"open source OK: {len(REQUIRED)} documents, "
          f"{len(CRITICAL_COMPONENTS)} critical components buildable from source")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
