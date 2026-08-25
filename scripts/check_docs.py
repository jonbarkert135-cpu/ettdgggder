#!/usr/bin/env python3
"""Documentation gate (roadmap item 72).

Three ways documentation rots, all cheap to detect and expensive to notice by
hand:

  * **A required document goes missing** — or was never written. Item 72 names
    the minimum set; `check_open_source.py` already checks the contents of the
    ones it cares about, this checks the set is complete.
  * **A link points at nothing.** A moved file leaves a trail of dead links, and
    the reader who finds one stops trusting the rest of the page.
  * **A document exists and nobody can find it.** Every file under docs/ is
    listed in docs/README.md, so the index cannot quietly fall behind the tree.

Usage: python3 scripts/check_docs.py [--selftest]
"""

from __future__ import annotations

import pathlib
import re
import sys
import urllib.parse

REPO = pathlib.Path(__file__).resolve().parent.parent
DOCS = REPO / "docs"
INDEX = DOCS / "README.md"

# The minimum set from item 72. Paths are relative to the repository root;
# CONTRIBUTING and SECURITY live at the root because that is where GitHub
# surfaces them, and docs/README.md says so.
REQUIRED = [
    "README.md",
    "CONTRIBUTING.md",
    "SECURITY.md",
    "docs/ARCHITECTURE.md",
    "docs/BUILD.md",
    "docs/security/THREAT_MODEL.md",
    "docs/PRIVACY.md",
    "docs/LICENSING.md",
    "docs/THIRD_PARTY.md",
    "docs/UPSTREAM_SYNC.md",
    "docs/RELEASES.md",
    "docs/SUPPLY_CHAIN.md",
    "docs/privacy/FILTER_LISTS.md",
]

LINK = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
# Fenced code blocks hold example paths that need not exist.
FENCE = re.compile(r"```.*?```", re.S)


def markdown_files() -> list[pathlib.Path]:
    skip = {".git", ".worktrees", "out", "third_party"}
    return [
        path
        for path in sorted(REPO.rglob("*.md"))
        if not skip & set(path.relative_to(REPO).parts)
    ]


def broken_links(path: pathlib.Path) -> list[str]:
    text = FENCE.sub("", path.read_text(encoding="utf-8"))
    broken = []
    for target in LINK.findall(text):
        target = target.strip()
        if target.startswith(("http://", "https://", "mailto:", "#")):
            continue
        target = urllib.parse.unquote(target.split("#")[0])
        if not target:
            continue
        if not (path.parent / target).exists():
            broken.append(target)
    return broken


def main() -> int:
    if "--selftest" in sys.argv:
        assert LINK.findall("see [x](docs/A.md) and [y](../B.md)") == ["docs/A.md", "../B.md"]
        assert LINK.findall(FENCE.sub("", "```\n[a](nope.md)\n```\n[b](docs/README.md)")) == [
            "docs/README.md"
        ], "code blocks are not links"
        assert broken_links(INDEX) == [], "the index links must resolve"
        assert len(markdown_files()) > 10, "the file walk found nothing — check the skip list"
        print("selftest OK")
        return 0

    errors: list[str] = []

    for relative in REQUIRED:
        if not (REPO / relative).is_file():
            errors.append(f"required document missing: {relative}")

    checked = 0
    for path in markdown_files():
        checked += 1
        for target in broken_links(path):
            errors.append(f"{path.relative_to(REPO)}: link to {target} does not resolve")

    index_text = INDEX.read_text(encoding="utf-8")
    listed = {
        (INDEX.parent / urllib.parse.unquote(target.split("#")[0])).resolve()
        for target in LINK.findall(index_text)
        if not target.startswith(("http", "mailto:", "#"))
    }
    for path in sorted(DOCS.rglob("*.md")):
        if path == INDEX:
            continue
        # A directory of many similar files (ADRs, research, fingerprinting
        # surfaces, release notes) is listed once by an example link; requiring
        # 22 rows for 22 fingerprinting surfaces would make the index unusable.
        if path.resolve() in listed or any(
            parent.resolve() in {link.parent for link in listed} for parent in [path.parent]
        ):
            continue
        errors.append(
            f"docs/{path.relative_to(DOCS)} is not reachable from docs/README.md — "
            f"add a row, or a link to its directory"
        )

    if errors:
        print("docs check FAILED:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(
        f"docs OK: {len(REQUIRED)} required documents present, {checked} Markdown files with "
        f"resolving links, index covers docs/"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
