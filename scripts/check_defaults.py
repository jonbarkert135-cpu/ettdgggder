#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Gate: the shipped defaults are the ones that were specified and documented.

    python3 scripts/check_defaults.py
    python3 scripts/check_defaults.py --selftest

Roadmap items 83 and 84. Defaults are the settings almost everyone will ever
have, so they are the product. This gate holds three things together:

  * the twelve defaults item 84 lists exist in `settings/defaults.cc` with the
    stated value -- the list is written out here too, so this is a check rather
    than a mirror of the code it checks;
  * `docs/DEFAULTS.md` documents every one of them, and cannot promise a default
    the code does not set;
  * telemetry and crash-report upload are disabled and marked non-negotiable, so
    no axis of user control can trade them away.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "src_overrides" / "bedrock" / "settings" / "defaults.cc"
DOC = ROOT / "docs" / "DEFAULTS.md"

REQUIRED = [
    ("secure_browsing", "enabled"),
    ("https_upgrades", "enabled"),
    ("third_party_tracking_protection", "enabled"),
    ("third_party_cookies", "restricted"),
    ("fingerprint_protection", "balanced"),
    ("ad_and_tracker_blocking", "enabled"),
    ("telemetry", "disabled"),
    ("crash_reporting", "disabled"),
    ("webrtc_privacy", "enabled"),
    ("secure_dns", "configurable"),
    ("extension_permissions", "explicit"),
    ("site_permissions", "ask_when_needed"),
]
NON_NEGOTIABLE = {"telemetry", "crash_reporting"}
ROW = re.compile(r'\{"(?P<id>\w+)", "(?P<value>\w+)",\s*(?P<rest>.*?)(?P<negotiable>true|false)\},',
                 re.S)
# How the document names each setting, so the table can read like English while
# still being checked against the code.
DOC_LABELS = {
    "secure_browsing": "Secure browsing",
    "https_upgrades": "HTTPS upgrades",
    "third_party_tracking_protection": "Third-party tracking protection",
    "third_party_cookies": "Third-party cookies",
    "fingerprint_protection": "Fingerprint protection",
    "ad_and_tracker_blocking": "Ad and tracker blocking",
    "telemetry": "Telemetry",
    "crash_reporting": "Crash reporting",
    "webrtc_privacy": "WebRTC privacy",
    "secure_dns": "Secure DNS",
    "extension_permissions": "Extension permissions",
    "site_permissions": "Site permissions",
}


def parse_defaults(text: str) -> list[dict]:
    return [
        {"id": match.group("id"), "value": match.group("value"),
         "rationale": match.group("rest"),
         "negotiable": match.group("negotiable") == "true"}
        for match in ROW.finditer(text)
    ]


def check(rows: list[dict], doc: str) -> list[str]:
    errors = []
    by_id = {row["id"]: row for row in rows}
    if len(rows) != len(REQUIRED):
        errors.append(f"defaults.cc: {len(rows)} defaults, item 84 lists {len(REQUIRED)}")
    for index, (setting_id, value) in enumerate(REQUIRED):
        row = by_id.get(setting_id)
        if not row:
            errors.append(f"{setting_id}: missing from defaults.cc")
            continue
        if row["value"] != value:
            errors.append(f"{setting_id}: ships as {row['value']!r}, item 84 says {value!r}")
        if len(row["rationale"]) < 60:
            errors.append(f"{setting_id}: the default has no stated reason")
        if index < len(rows) and rows[index]["id"] != setting_id:
            errors.append(f"{setting_id}: out of the order item 84 lists")
    for setting_id in NON_NEGOTIABLE:
        row = by_id.get(setting_id)
        if row and row["negotiable"]:
            errors.append(f"{setting_id}: marked negotiable — no axis may trade it away")
        if row and row["value"] != "disabled":
            errors.append(f"{setting_id}: must ship disabled")

    if "Balanced Privacy" not in doc:
        errors.append("docs/DEFAULTS.md: does not name the shipped profile")
    for setting_id, value in REQUIRED:
        label = DOC_LABELS[setting_id]
        if label not in doc:
            errors.append(f"docs/DEFAULTS.md: does not document {label!r}")
        else:
            line = next((line for line in doc.splitlines() if line.startswith(f"| {label} |")), "")
            documented = value.replace("_", " ")
            if documented not in line.lower():
                errors.append(
                    f"docs/DEFAULTS.md: {label!r} is documented as something other "
                    f"than {value!r}")
    return errors


def selftest() -> int:
    sample = '{"telemetry", "disabled",\n "' + "x" * 70 + '",\n true},\n'
    rows = parse_defaults(sample)
    assert rows and rows[0]["negotiable"], "parser reads the negotiable flag"
    errors = check(rows, "Balanced Privacy Telemetry")
    assert any("negotiable" in error for error in errors), errors
    assert any("item 84 lists" in error for error in errors), errors
    print("check_defaults selftest OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    if parser.parse_args().selftest:
        return selftest()

    rows = parse_defaults(SOURCE.read_text(encoding="utf-8"))
    errors = check(rows, DOC.read_text(encoding="utf-8"))
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1
    print(f"defaults OK: {len(rows)} shipped defaults match item 84 and docs/DEFAULTS.md, "
          f"{len(NON_NEGOTIABLE)} of them outside every axis of user control")
    return 0


if __name__ == "__main__":
    sys.exit(main())
