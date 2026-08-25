#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Gate: every shipped DNS preset is an endpoint, and none has gone stale.

    python3 scripts/check_dns_presets.py
    python3 scripts/check_dns_presets.py --selftest

Audit finding F6b: the shipped `dns0.eu` preset pointed at `https://dns0.eu/` --
the operator's *website*, not a DoH endpoint. With fallback enabled, a preset
that cannot answer means the query goes out in plaintext, so a typo in this list
is a privacy failure rather than a broken feature. Worse, by the time the audit
ran, the operator had shut the service down entirely (October 2025), and nothing
in the build had any way to notice.

A resolver list is perishable, and CI cannot phone the internet. So each preset
carries the date a human last checked it against the operator's documentation,
and this gate fails when one of them is older than the window below -- the build
asks for a re-check instead of shipping a stale list forever.

What is checked, all offline:
  * the DoH template is https:// and has a path (a bare origin is F6b);
  * no preset resolver belongs to this project;
  * every preset records an ISO `verified` date, and none is stale.
"""

from __future__ import annotations

import argparse
import datetime
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "src_overrides" / "bedrock" / "privacy" / "network" / "dns_settings.cc"
MAX_AGE_DAYS = 365
# Entries look like: {"Name", "Operator", "https://…/dns-query", "https://…",
#                     false, true, "2026-08-25"},
ENTRY = re.compile(
    r'\{"(?P<name>[^"]+)",\s*"(?P<operator>[^"]*)",\s*'
    r'"(?P<doh>[^"]*)",\s*"(?P<policy>[^"]*)",\s*'
    r'(?:true|false),\s*(?:true|false),\s*"(?P<verified>[^"]*)"\},',
    re.S,
)


def parse(text: str) -> list[dict]:
    return [match.groupdict() for match in ENTRY.finditer(text)]


def check(presets: list[dict], today: datetime.date) -> list[str]:
    errors = []
    if not presets:
        return ["no DNS presets found -- has the list moved?"]
    for preset in presets:
        name = preset["name"]
        doh = preset["doh"]
        if not doh.startswith("https://"):
            errors.append(f"{name}: DoH template is not https://")
        elif "/" not in doh[len("https://"):].rstrip("/"):
            # F6b exactly: an origin with no path is a website, not an endpoint.
            errors.append(
                f"{name}: {doh} is an origin, not a DoH endpoint -- a preset "
                "that cannot answer means plaintext DNS when fallback is on"
            )
        if "bedrock" in doh.lower():
            errors.append(f"{name}: no resolver may belong to this project")
        if not preset["operator"] or not preset["policy"]:
            errors.append(f"{name}: a preset must name its operator and policy")
        try:
            verified = datetime.date.fromisoformat(preset["verified"])
        except ValueError:
            errors.append(f"{name}: verified date is not an ISO date")
            continue
        age = (today - verified).days
        if age > MAX_AGE_DAYS:
            errors.append(
                f"{name}: last checked {age} days ago -- confirm the endpoint "
                "against the operator's own documentation and update the date "
                "(dns0.eu was shipped for months after it shut down)"
            )
        if age < 0:
            errors.append(f"{name}: verified date is in the future")
    return errors


def selftest() -> int:
    today = datetime.date(2026, 8, 25)
    good = '{"Quad9", "Quad9", "https://dns.quad9.net/dns-query", "https://q/", false, true, "2026-08-25"},'
    assert check(parse(good), today) == [], check(parse(good), today)
    website = '{"dns0.eu", "dns0", "https://dns0.eu/", "https://p/", false, true, "2026-08-25"},'
    assert any("not a DoH endpoint" in e for e in check(parse(website), today)), "F6b is caught"
    stale = '{"Old", "Op", "https://old.test/dns-query", "https://p/", false, false, "2024-01-01"},'
    assert any("last checked" in e for e in check(parse(stale), today)), "staleness is caught"
    ours = '{"Ours", "Op", "https://dns.bedrock.test/dns-query", "https://p/", false, false, "2026-08-25"},'
    assert any("belong to this project" in e for e in check(parse(ours), today))
    assert check([], today), "an empty list is an error, not a pass"
    print("check_dns_presets selftest OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    if parser.parse_args().selftest:
        return selftest()

    presets = parse(SOURCE.read_text(encoding="utf-8"))
    errors = check(presets, datetime.date.today())
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1
    print(f"DNS presets OK: {len(presets)} resolvers, each an endpoint with a "
          f"named operator, all re-checked within {MAX_AGE_DAYS} days")
    return 0


if __name__ == "__main__":
    sys.exit(main())
