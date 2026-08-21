#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Fail if the language policy of ADR 0004 is broken.

Checkable parts of item 48: no Electron and no shipped Node/Python runtime, web
languages only in WebUI directories, and Rust only behind a documented FFI
boundary with a provenance row.

Run: python3 scripts/check_languages.py [--selftest]
"""

from __future__ import annotations

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SOURCES = REPO / "src_overrides"

# WebUI lives with the surface it renders. Everything else is C++ (or Rust).
WEB_DIRS = ("bedrock/settings", "bedrock/ui", "bedrock/devtools")
WEB_SUFFIXES = {".ts", ".tsx", ".html", ".css", ".js"}
ALLOWED_SUFFIXES = {".h", ".cc", ".json", ".rs", ".toml", ".gn", ".gni", ".md"} | WEB_SUFFIXES

# Substrings that mean a shipped runtime, not a build-time tool.
BANNED = {
    "electron": "Electron is not the shell (ADR 0004); the shell is Chromium",
    "nw.js": "NW.js is a browser shell we do not use",
    "node_modules": "no Node runtime ships with Bedrock",
}
# Files where the words above are the subject, not a dependency.
PROSE = {".md", ".txt"}


def check_sources(errors: list[str]) -> None:
    for path in sorted(SOURCES.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(SOURCES).as_posix()
        if path.suffix not in ALLOWED_SUFFIXES:
            errors.append(f"{rel}: unexpected language for this tree (ADR 0004)")
        if path.suffix in WEB_SUFFIXES and not rel.startswith(WEB_DIRS):
            errors.append(f"{rel}: web languages belong to a WebUI surface ({', '.join(WEB_DIRS)})")


def check_rust(errors: list[str]) -> None:
    crates = sorted(SOURCES.rglob("Cargo.toml"))
    inventory = (REPO / "docs" / "THIRD_PARTY.md").read_text(encoding="utf-8")
    for crate in crates:
        directory = crate.parent
        rel = directory.relative_to(SOURCES).as_posix()
        if not (directory / "src" / "ffi.rs").is_file():
            errors.append(f"{rel}: Rust crate without src/ffi.rs — ADR 0004 requires one door")
        for source in directory.rglob("*.rs"):
            if source.name == "ffi.rs":
                continue
            if "unsafe" in source.read_text(encoding="utf-8"):
                errors.append(f"{source.relative_to(SOURCES)}: unsafe outside ffi.rs")
        if directory.name not in inventory:
            errors.append(f"{rel}: no provenance row in docs/THIRD_PARTY.md")


def check_banned(errors: list[str]) -> None:
    for path in sorted(REPO.rglob("*")):
        if not path.is_file() or ".git" in path.parts or path.suffix in PROSE:
            continue
        if path == Path(__file__).resolve():  # this file names them on purpose
            continue
        try:
            text = path.read_text(encoding="utf-8").lower()
        except (UnicodeDecodeError, OSError):
            continue
        for needle, why in BANNED.items():
            if needle in text:
                errors.append(f"{path.relative_to(REPO)}: {why}")


def main() -> int:
    if "--selftest" in sys.argv:
        errors: list[str] = []
        check_sources(errors)
        if errors:
            print("selftest: clean tree reports errors:", *errors, sep="\n  ")
            return 1
        print("selftest: ok")
        return 0

    errors = []
    check_sources(errors)
    check_rust(errors)
    check_banned(errors)
    if errors:
        print("language policy check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    rust = len(list(SOURCES.rglob("Cargo.toml")))
    print(f"languages OK: C++ tree clean, {rust} Rust crates, no Electron or shipped runtime")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
