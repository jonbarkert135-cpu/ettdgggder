#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Gate: architecture decisions are recorded, complete, and indexed.

    python3 scripts/check_adr.py
    python3 scripts/check_adr.py --selftest

Roadmap item 86. An ADR that exists but is not in the index is a decision nobody
will find; an ADR without an "alternatives considered" section is a description
of what was built, not a record of a decision. Both are checked here, along with
the ten decisions item 86 names by its own numbering -- Bedrock's files are
numbered in the order the decisions were made, so the index carries the mapping
and this gate verifies each of the ten is actually mapped to a file that exists.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ADR_DIR = ROOT / "docs" / "adr"
INDEX = ADR_DIR / "README.md"

REQUIRED_SECTIONS = ["## Context", "## Decision", "## Consequences"]
ALTERNATIVES = re.compile(r"^##+ .*Alternatives", re.M | re.IGNORECASE)
STATUS = re.compile(r"^\*\*Status:\*\* (?P<status>accepted|superseded|proposed)", re.M)
# The ten decisions item 86 asks for, by the words it uses.
ROADMAP_TOPICS = [
    "Chromium as base", "Privacy architecture", "Fingerprinting strategy",
    "Content blocker", "Search architecture", "Tor integration",
    "Storage isolation", "Theme architecture", "Update strategy",
    "License strategy",
]
LINK = re.compile(r"\]\((?P<target>\d{4}-[a-z0-9-]+\.md)\)")


def records() -> list[pathlib.Path]:
    return sorted(path for path in ADR_DIR.glob("*.md") if path.name != "README.md")


def check_record(path: pathlib.Path, errors: list[str]) -> None:
    text = path.read_text(encoding="utf-8")
    name = path.name
    if not text.startswith("# ADR "):
        errors.append(f"{name}: does not start with an 'ADR NNNN' title")
    if not STATUS.search(text):
        errors.append(f"{name}: no **Status:** line (accepted / superseded / proposed)")
    for section in REQUIRED_SECTIONS:
        if section not in text:
            errors.append(f"{name}: missing section {section!r}")
    if not ALTERNATIVES.search(text):
        errors.append(f"{name}: no 'Alternatives considered' — a record without a rejected "
                      "option is a description, not a decision")
    number = name[:4]
    if not re.match(r"^\d{4}$", number):
        errors.append(f"{name}: filename does not start with a four-digit number")
    elif f"# ADR {number}" not in text:
        errors.append(f"{name}: title number does not match the filename")


def check_index(errors: list[str]) -> None:
    if not INDEX.is_file():
        errors.append("docs/adr/README.md: missing")
        return
    text = INDEX.read_text(encoding="utf-8")
    linked = set(LINK.findall(text))
    for path in records():
        if path.name not in linked:
            errors.append(f"{path.name}: exists but is not in the index")
    for target in linked:
        if not (ADR_DIR / target).is_file():
            errors.append(f"docs/adr/README.md: links to {target}, which does not exist")
    for topic in ROADMAP_TOPICS:
        if topic.lower() not in text.lower():
            errors.append(f"docs/adr/README.md: item 86 asks for {topic!r}, not mapped")
        else:
            line = next((line for line in text.splitlines()
                         if topic.lower() in line.lower() and "](" in line), "")
            if not LINK.search(line):
                errors.append(f"docs/adr/README.md: {topic!r} is named but links to no record")


def selftest() -> int:
    errors: list[str] = []
    for path in records():
        check_record(path, errors)
    assert not errors, f"every record in the tree must pass: {errors}"
    assert ALTERNATIVES.search("## Alternatives considered\n"), "section matcher works"
    assert not ALTERNATIVES.search("Alternatives were discussed\n"), "prose is not a section"
    assert LINK.findall("[0007](0007-privacy-architecture.md)") == \
        ["0007-privacy-architecture.md"]
    print("check_adr selftest OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    if parser.parse_args().selftest:
        return selftest()

    errors: list[str] = []
    for path in records():
        check_record(path, errors)
    check_index(errors)
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1
    print(f"ADRs OK: {len(records())} records, each with context, decision, alternatives and "
          f"consequences; all {len(ROADMAP_TOPICS)} decisions item 86 names are mapped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
