#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Fail if the browser would claim something it does not do (roadmap item 55).

Three ways a browser lies, and the check for each:

  1. A switch for a protection that is not enforced. The feature registry
     records a Status per feature; only `kEnforced` may be rendered, and a
     feature may only be marked enforced once `build/ENFORCEMENT.md` records
     the build that enforces it.
  2. A number that no event produced. Any figure with a thousands separator in
     user-visible text (mockups, UI headers, UI docs) must sit next to a
     "sample"/"example" marker.
  3. A claim that cannot be proven: anonymous, untraceable, invisible,
     unhackable, "100% private", military-grade.

Run: python3 scripts/check_no_fake_features.py [--selftest]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
REGISTRY = REPO / "src_overrides" / "bedrock" / "privacy" / "core" / "privacy_engine.cc"
ENFORCEMENT = REPO / "build" / "ENFORCEMENT.md"

# Tests are not user-visible copy — several of them exist precisely to ban these
# words, and flagging their fixtures would punish the check for working.
UI_SOURCES = [
    REPO / "src_overrides" / "bedrock" / "ui",
    REPO / "src_overrides" / "bedrock" / "settings",
    REPO / "src_overrides" / "bedrock" / "devtools",
    REPO / "src_overrides" / "bedrock" / "privacy" / "stats",
]
MOCKUPS = sorted((REPO / "docs" / "design" / "mockups").glob("*.html"))

CLAIMS = [
    r"\banonymous\b",
    r"\banonymity\b",
    r"\buntraceable\b",
    r"\binvisible\b",
    r"\bunhackable\b",
    r"\bbulletproof\b",
    r"military[- ]grade",
    r"100%\s*(private|secure|anonymous|protected)",
    r"\bcompletely\s+(private|secure|protected)\b",
]
# Text that is *about* the claim rather than making it. A file discussing why we
# never say "anonymous" is the opposite of the problem.
DISCUSSION = ("never", "not anonymous", "no anonymity", "does not", "is not", "refuse",
              "cannot claim", "forbidden", "must not", "prohibition", "temptation",
              "banned", "item 51", "item 55")
SAMPLE = ("sample", "example", "illustrative", "mock", "not measured", "placeholder")
NUMBER = re.compile(r"\b\d{1,3}(?:,\d{3})+\b")


def strip_css_js(html: str) -> str:
    html = re.sub(r"<style.*?</style>", " ", html, flags=re.S)
    html = re.sub(r"<script.*?</script>", " ", html, flags=re.S)
    html = re.sub(r"<!--.*?-->", " ", html, flags=re.S)
    return re.sub(r"<[^>]+>", " ", html)


def visible_texts() -> list[tuple[str, str]]:
    """(where, text) pairs of user-visible copy."""
    out: list[tuple[str, str]] = []
    for path in MOCKUPS:
        out.append((str(path.relative_to(REPO)), strip_css_js(path.read_text(encoding="utf-8"))))
    for directory in UI_SOURCES:
        for path in sorted(directory.rglob("*")):
            if path.suffix in (".h", ".cc") and not path.name.endswith("_test.cc"):
                out.append((str(path.relative_to(REPO)), path.read_text(encoding="utf-8")))
    return out


def check_claims(errors: list[str]) -> None:
    for where, text in visible_texts():
        for line_no, line in enumerate(text.splitlines(), start=1):
            lowered = line.lower()
            if any(marker in lowered for marker in DISCUSSION):
                continue
            for pattern in CLAIMS:
                if re.search(pattern, lowered):
                    errors.append(f"{where}:{line_no}: unprovable claim — {line.strip()[:90]!r}")


def check_numbers(errors: list[str]) -> None:
    for where, text in visible_texts():
        lines = text.splitlines()
        for line_no, line in enumerate(lines, start=1):
            if not NUMBER.search(line):
                continue
            window = " ".join(lines[max(0, line_no - 11):line_no + 10]).lower()
            if not any(marker in window for marker in SAMPLE):
                errors.append(
                    f"{where}:{line_no}: a counter with no event behind it — "
                    f"mark it as sample data or delete it: {line.strip()[:70]!r}")


def check_enforced_features(errors: list[str]) -> None:
    text = REGISTRY.read_text(encoding="utf-8")
    enforced = re.findall(r'"([a-z0-9_]+)",\s*\n?[^}]*?Status::kEnforced', text)
    if not enforced:
        return
    if not ENFORCEMENT.is_file():
        errors.append(
            "features are marked kEnforced but build/ENFORCEMENT.md does not exist — "
            "a feature is enforced only when a build performs it")
        return
    record = ENFORCEMENT.read_text(encoding="utf-8")
    for feature in enforced:
        if feature not in record:
            errors.append(f"{feature}: marked kEnforced but not recorded in build/ENFORCEMENT.md")


def main() -> int:
    errors: list[str] = []
    check_claims(errors)
    check_numbers(errors)
    check_enforced_features(errors)
    if errors:
        print("fake feature check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("no fake features: no unprovable claims, no unbacked counters, no unenforced switches")
    return 0


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        assert NUMBER.search("Trackers blocked 12,481"), "counter pattern broken"
        assert not NUMBER.search("blocked 481 requests"), "plain numbers must not trip the check"
        assert re.search(CLAIMS[0], "you are anonymous here"), "claim pattern broken"
        assert strip_css_js("<style>a{}</style><p>hi</p>").strip() == "hi", "html strip broken"
        print("selftest OK")
        raise SystemExit(0)
    raise SystemExit(main())
