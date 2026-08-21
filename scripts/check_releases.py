#!/usr/bin/env python3
"""Release channel and release-notes gate (roadmap item 71).

The channel table in docs/RELEASES.md and the numbers in release_channels.cc
have to be the same numbers. They will not stay that way on their own: one of
them is edited during a release, the other during a refactor, and the day they
disagree is the day someone promotes a build a week early because the document
said seven.

Also checks that every release note carries all six fields item 71 requires —
including "known issues", the one that gets dropped first.

Usage: python3 scripts/check_releases.py [--selftest]
"""

from __future__ import annotations

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
CODE = REPO / "src_overrides" / "bedrock" / "updater" / "release_channels.cc"
DOC = REPO / "docs" / "RELEASES.md"
NOTES = REPO / "docs" / "releases"

CHANNELS = ["nightly", "beta", "stable"]

# The six fields, as the section headings a note is expected to use.
FIELDS = [
    "version number",
    "chromium base version",
    "security fixes",
    "privacy changes",
    "dependencies",
    "known issues",
]


def code_channels(text: str) -> dict[str, dict[str, object]]:
    """cadence_days / soak_days / accepts_direct_landings per channel, from the switch."""
    channels: dict[str, dict[str, object]] = {}
    for name in CHANNELS:
        case = re.search(
            rf"case Channel::k{name.capitalize()}:(.*?)break;", text, re.S
        )
        if not case:
            sys.exit(f"release_channels.cc: no case for {name}")
        body = case.group(1)

        def field(key: str, default: str = "") -> str:
            match = re.search(rf"properties\.{key} = ([^;]+);", body)
            return match.group(1).strip() if match else default

        channels[name] = {
            "cadence_days": int(field("cadence_days", "0")),
            "soak_days": int(field("soak_days", "0")),
            "direct": field("accepts_direct_landings", "false") == "true",
            "suffix": field("version_suffix_prefix", '""').strip('"'),
        }
    return channels


def doc_channels(text: str) -> dict[str, list[str]]:
    rows = {}
    for line in text.splitlines():
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) >= 5 and cells[0] in CHANNELS:
            rows[cells[0]] = cells
    return rows


def check(errors: list[str]) -> tuple[int, int]:
    code = code_channels(CODE.read_text(encoding="utf-8"))
    doc = doc_channels(DOC.read_text(encoding="utf-8"))

    for name in CHANNELS:
        if name not in doc:
            errors.append(f"docs/RELEASES.md has no table row for the {name} channel")
            continue
        cadence, suffix, direct, soak = doc[name][1:5]
        if str(code[name]["cadence_days"]) not in cadence:
            errors.append(
                f"{name}: code cadence is {code[name]['cadence_days']} days, "
                f"docs/RELEASES.md says '{cadence}'"
            )
        stated_direct = direct.lower().startswith("yes")
        if stated_direct != code[name]["direct"]:
            errors.append(
                f"{name}: code {'allows' if code[name]['direct'] else 'refuses'} direct landings, "
                f"the table says '{direct}'"
            )
        if code[name]["soak_days"] and str(code[name]["soak_days"]) not in soak:
            errors.append(
                f"{name}: code soak is {code[name]['soak_days']} days, the table says '{soak}'"
            )
        has_suffix = bool(code[name]["suffix"])
        if has_suffix != (suffix.lower() not in ("none", "—", "-", "")):
            errors.append(f"{name}: version suffix disagrees between code and the table")

    # Soak lengthens as the audience widens — a table that says otherwise is a
    # funnel that does nothing.
    if code["nightly"]["soak_days"] >= code["beta"]["soak_days"]:
        errors.append("nightly soak is not shorter than beta soak")

    for field in FIELDS:
        if field not in DOC.read_text(encoding="utf-8").lower():
            errors.append(f"docs/RELEASES.md does not list the required field '{field}'")

    notes = sorted(NOTES.glob("*.md")) if NOTES.exists() else []
    if not notes:
        errors.append("docs/releases/ has no TEMPLATE.md")
    for path in notes:
        text = path.read_text(encoding="utf-8").lower()
        for field in FIELDS:
            if not re.search(rf"^#+\s*{re.escape(field)}\s*$", text, re.M):
                errors.append(f"docs/releases/{path.name}: no '{field}' section")
        if path.name != "TEMPLATE.md" and "known issues" in text:
            body = text.split("known issues", 1)[1].strip(" #\n-")
            if not body:
                errors.append(f"docs/releases/{path.name}: 'known issues' section is empty")

    return len(code), len(notes)


def main() -> int:
    if "--selftest" in sys.argv:
        parsed = code_channels(
            "case Channel::kNightly:\n properties.cadence_days = 1;\n"
            ' properties.version_suffix_prefix = "-nightly.";\n'
            " properties.accepts_direct_landings = true;\n"
            " properties.soak_days = 7;\n break;\n"
            "case Channel::kBeta:\n properties.cadence_days = 7;\n"
            ' properties.version_suffix_prefix = "-beta.";\n'
            " properties.accepts_direct_landings = false;\n properties.soak_days = 14;\n break;\n"
            "case Channel::kStable:\n properties.cadence_days = 28;\n"
            ' properties.version_suffix_prefix = "";\n'
            " properties.accepts_direct_landings = false;\n properties.soak_days = 0;\n break;\n"
        )
        assert parsed["nightly"]["direct"] and not parsed["beta"]["direct"], parsed
        assert parsed["stable"]["suffix"] == "", parsed
        assert doc_channels("| nightly | 1 day | `-n.` | yes | 7 days | devs |")["nightly"][3] == (
            "yes"
        )
        assert len(FIELDS) == 6
        print("selftest OK")
        return 0

    errors: list[str] = []
    channels, notes = check(errors)
    if errors:
        print("releases check FAILED:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(
        f"releases OK: {channels} channels match docs/RELEASES.md, {notes} note file(s) carry all "
        f"{len(FIELDS)} required fields"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
