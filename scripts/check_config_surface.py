#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Fail if the configuration surface and its documentation disagree (item 56).

The C++ table is the source of truth; `docs/CONFIGURATION.md` must describe
exactly it — every key, every switch, every allowed value. An undocumented
switch is an undiscoverable feature, and a documented switch that does not
exist is a lie in the manual.

Run: python3 scripts/check_config_surface.py [--selftest]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TABLE = REPO / "src_overrides" / "bedrock" / "settings" / "config_surface.cc"
DOC = REPO / "docs" / "CONFIGURATION.md"
GUARDS = REPO / "src_overrides" / "bedrock" / "settings" / "advanced_settings.cc"
FORMATS_CODE = REPO / "src_overrides" / "bedrock" / "settings" / "portability.cc"
FORMATS_DOC = REPO / "docs" / "FORMATS.md"

ROW = re.compile(
    r'\{"(?P<key>[a-z0-9_.]+)",\s*"(?P<switch>[a-z0-9-]*)",\s*"(?P<policy>[A-Za-z]*)",'
    r'\s*(?P<rest>.*?)\},\s*\n',
    re.S,
)
VALUES = re.compile(r"\{(?P<values>(?:\s*\"[a-z0-9-]+\",?)*)\s*\}")


def parse_table() -> list[dict[str, object]]:
    text = TABLE.read_text(encoding="utf-8")
    body = text.split("static const std::vector<SettingSpec> table = {", 1)[1]
    rows = []
    for match in ROW.finditer(body):
        values_match = VALUES.search(match.group("rest"))
        values = re.findall(r'"([a-z0-9-]+)"', values_match.group("values")) if values_match else []
        rows.append(
            {
                "key": match.group("key"),
                "switch": match.group("switch"),
                "policy": match.group("policy"),
                "values": values,
            }
        )
    return rows


def check_guards(errors: list[str]) -> int:
    """Item 57: every guard in code is documented, and vice versa."""
    code = set(re.findall(r'\{"(G\d+)",', GUARDS.read_text(encoding="utf-8")))
    doc = set(re.findall(r"^\| (G\d+) \|", DOC.read_text(encoding="utf-8"), re.M))
    for guard in sorted(code - doc):
        errors.append(f"{guard}: guard exists in code but is undocumented")
    for guard in sorted(doc - code):
        errors.append(f"{guard}: documented but no such guard in advanced_settings.cc")
    return len(code)


def check_formats(errors: list[str]) -> int:
    """Item 59: every export format is documented with its id and extension."""
    text = FORMATS_CODE.read_text(encoding="utf-8")
    doc = FORMATS_DOC.read_text(encoding="utf-8")
    entries = re.findall(r"\{Payload::k\w+, (.*?)\},\n", text, re.S)
    ids = []
    for entry in entries:
        # C++ splits long literals across lines; join them before comparing.
        literals = [re.sub(r'"\s*\n\s*"', "", literal)
                    for literal in re.findall(r'"((?:[^"\\]|\\.)*)"', entry)]
        joined = " ".join(literals)
        format_id = next((value for value in literals if value.startswith("bedrock.")), "")
        ids.append(format_id)
        extension = next((value for value in literals if value.startswith(".")), "")
        for value, what in ((format_id, "id"), (extension, "extension")):
            if value and value not in doc:
                errors.append(f"{format_id}: {what} {value!r} missing from docs/FORMATS.md")
        if format_id and joined.count(" ") < 5:
            errors.append(f"{format_id}: format row has no description")
    for documented in set(re.findall(r"`(bedrock\.[a-z-]+\.v\d+)`", doc)):
        if documented not in ids:
            errors.append(f"{documented}: documented but not in portability.cc")
    if len(entries) < 5:
        errors.append(f"only parsed {len(entries)} formats from portability.cc — parser broke")
    return len(entries)


def main() -> int:
    rows = parse_table()
    doc = DOC.read_text(encoding="utf-8")
    errors: list[str] = []

    if len(rows) < 10:
        errors.append(f"only parsed {len(rows)} settings from config_surface.cc — parser or table broke")

    for row in rows:
        if f"`{row['key']}`" not in doc:
            errors.append(f"{row['key']}: not documented in docs/CONFIGURATION.md")
        if row["switch"] and f"--{row['switch']}" not in doc:
            errors.append(f"--{row['switch']}: switch exists in code but is undocumented")
        if row["policy"] and row["policy"] not in doc:
            errors.append(f"{row['policy']}: policy exists in code but is undocumented")
        for value in row["values"]:
            if value not in doc:
                errors.append(f"{row['key']}: allowed value {value!r} is undocumented")

    guards = check_guards(errors)
    formats = check_formats(errors)

    known_switches = {f"--{row['switch']}" for row in rows if row["switch"]}
    for documented in set(re.findall(r"`(--[a-z-]+)[=`]", doc)):
        if documented not in known_switches and documented != "--help":
            errors.append(f"{documented}: documented but not in config_surface.cc")

    if errors:
        print("configuration surface check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    switches = sum(1 for row in rows if row["switch"])
    print(f"configuration OK: {len(rows)} settings, {switches} documented switches, "
          f"{guards} guards, {formats} portable formats")
    return 0


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        parsed = parse_table()
        assert parsed, "the table parser found nothing"
        assert any(row["key"] == "privacy.level" for row in parsed), "privacy.level row missing"
        assert any("balanced" in row["values"] for row in parsed), "allowed values not parsed"
        probe: list[str] = []
        assert check_guards(probe) >= 8 and not probe, f"guard check broken: {probe}"
        assert check_formats(probe) == 5 and not probe, f"format check broken: {probe}"
        print("selftest OK")
        raise SystemExit(0)
    raise SystemExit(main())
