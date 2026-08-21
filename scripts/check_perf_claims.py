#!/usr/bin/env python3
"""Fail if the project makes a performance claim without a number.
Run: python3 scripts/check_perf_claims.py

Roadmap item 46 is explicit: "performance is good" is not allowed, measurements
are. Marketing adjectives are the easiest thing in the world to write and the
hardest to verify later, so they are a build error here.

A sentence may use these words if it also contains a number with a unit — that
is a claim someone can check.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SEARCH_DIRS = [REPO / "docs", REPO / "README.md", REPO / "CONTRIBUTING.md",
               REPO / "src_overrides"]

BANNED = [
    "performance is good", "performance is great", "blazing fast",
    "lightning fast", "buttery smooth", "no performance impact",
    "zero overhead", "negligible impact", "as fast as possible",
    "extremely fast", "super fast", "insanely fast", "no measurable cost",
]

# A number followed by a unit somewhere in the same sentence makes it a claim.
NUMBER = re.compile(
    r"\d+(\.\d+)?\s?(ms|us|µs|s|ns|%|mb|gb|kb|x|×|k rules|rules)", re.I)

SOFT = ["fast", "faster", "fastest", "quick", "instant", "efficient",
        "lightweight", "low overhead", "high performance"]


def sentences(text: str) -> list[str]:
    return re.split(r"(?<=[.!?\n|])\s+", text)


def main() -> int:
    errors: list[str] = []
    files: list[Path] = []
    for target in SEARCH_DIRS:
        if target.is_file():
            files.append(target)
        elif target.is_dir():
            files.extend(p for p in target.rglob("*")
                         if p.suffix in {".md", ".cc", ".h"})

    for path in sorted(files):
        text = path.read_text()
        lower = text.lower()
        for phrase in BANNED:
            # Quoting the phrase to explain why it is banned is allowed; making
            # the claim is not.
            quoted = any(q + phrase + q in lower or
                         q + phrase in lower
                         for q in ('"', "'", "\u201c", "\u00ab"))
            if phrase in lower and not quoted:
                errors.append(
                    f"{path.relative_to(REPO)}: unverifiable claim {phrase!r} — "
                    f"give a number and a method (docs/performance/BUDGETS.md)")
        for sentence in sentences(text):
            low = sentence.lower()
            if NUMBER.search(low):
                continue
            for word in SOFT:
                # Only flag a superlative claim about *this* browser, not the
                # ordinary use of the word ("a fast path", "efficient enough").
                if re.search(rf"\b(bedrock|it|the browser)\b[^.]*\b{word}\b", low):
                    errors.append(
                        f"{path.relative_to(REPO)}: \"{sentence.strip()[:80]}\" "
                        f"claims speed with no measurement")
                    break

    if errors:
        print("performance claim check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(f"performance claims OK: {len(files)} files, every speed claim carries a number")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
