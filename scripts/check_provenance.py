#!/usr/bin/env python3
"""Fail if the licensing record is incomplete. Run: python3 scripts/check_provenance.py

Enforces docs/LICENSING.md section 7:
  1. inventory row <-> notice file are 1:1
  2. versions are pinned (except reuse mode 'reimplement', which ships no code)
  3. GPL-family licenses may only be 'separate-artifact' or 'not-used'
  4. build/chromium.pin is well formed and matches the Chromium inventory row
  5. every patch under patches/upstream/<project>/ maps to a port/patched-base row
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
INVENTORY = REPO / "docs" / "THIRD_PARTY.md"
NOTICES = REPO / "THIRD_PARTY_NOTICES"
PIN = REPO / "build" / "chromium.pin"

MODES = {"patched-base", "port", "vendored", "separate-artifact", "reimplement", "not-used"}
UNPINNED = {"", "main", "master", "latest", "head", "trunk"}
COPYLEFT = ("GPL-2.0", "GPL-3.0", "AGPL")  # substring match; LGPL is handled below


def parse_inventory() -> list[dict[str, str]]:
    text = INVENTORY.read_text()
    block = re.search(r"<!-- BEGIN INVENTORY -->(.*?)<!-- END INVENTORY -->", text, re.S)
    if not block:
        sys.exit("THIRD_PARTY.md: INVENTORY markers not found")
    rows = []
    for line in block.group(1).strip().splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) != 6 or cells[0] in ("Project",) or set(cells[0]) <= set("- "):
            continue
        rows.append(dict(zip(
            ["project", "repository", "version", "license", "mode", "notice"], cells)))
    if not rows:
        sys.exit("THIRD_PARTY.md: inventory table is empty")
    return rows


def check(rows: list[dict[str, str]]) -> list[str]:
    errors: list[str] = []
    listed_notices = set()

    for row in rows:
        name = row["project"]
        listed_notices.add(row["notice"])

        if row["mode"] not in MODES:
            errors.append(f"{name}: unknown reuse mode {row['mode']!r} (allowed: {sorted(MODES)})")

        if not (NOTICES / row["notice"]).is_file():
            errors.append(f"{name}: missing notice file THIRD_PARTY_NOTICES/{row['notice']}")

        if not row["repository"].startswith("http"):
            errors.append(f"{name}: repository must be a URL")

        pinned = row["version"].lower() not in UNPINNED and row["version"] != "not-pinned-yet"
        if not pinned and row["mode"] != "reimplement":
            errors.append(
                f"{name}: version {row['version']!r} is not pinned, required for mode "
                f"{row['mode']!r} (only 'reimplement' may be unpinned)")

        lic = row["license"].upper()
        is_copyleft = any(c in lic for c in COPYLEFT) and "LGPL" not in lic
        if is_copyleft and row["mode"] not in ("separate-artifact", "not-used"):
            errors.append(
                f"{name}: {row['license']} may not be {row['mode']!r} — it would relicense the "
                f"browser binary. See docs/LICENSING.md section 3.")

    for notice in sorted(NOTICES.glob("*.txt")):
        if notice.name not in listed_notices:
            errors.append(f"THIRD_PARTY_NOTICES/{notice.name} has no row in docs/THIRD_PARTY.md")

    return errors


def check_pin(rows: list[dict[str, str]]) -> list[str]:
    errors = []
    pin = {}
    for line in PIN.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            key, _, value = line.partition("=")
            pin[key.strip()] = value.strip()

    if not re.fullmatch(r"\d+\.\d+\.\d+\.\d+", pin.get("version", "")):
        errors.append(f"chromium.pin: bad version {pin.get('version')!r}")
    if not re.fullmatch(r"[0-9a-f]{40}", pin.get("commit", "")):
        errors.append(f"chromium.pin: commit must be a 40-char sha, got {pin.get('commit')!r}")

    chromium = next((r for r in rows if r["project"] == "Chromium"), None)
    if chromium is None:
        errors.append("THIRD_PARTY.md: no Chromium row")
    elif chromium["version"] != pin.get("version"):
        errors.append(
            f"chromium.pin version {pin.get('version')} != THIRD_PARTY.md {chromium['version']}")
    return errors


def check_upstream_patches(rows: list[dict[str, str]]) -> list[str]:
    """patches/upstream/<slug>/ must correspond to a port/patched-base inventory row."""
    allowed = {
        re.sub(r"[^a-z0-9]+", "-", r["project"].lower()).strip("-")
        for r in rows if r["mode"] in ("port", "patched-base")
    }
    errors = []
    upstream = REPO / "patches" / "upstream"
    for directory in sorted(p for p in upstream.glob("*") if p.is_dir()):
        if not any(directory.rglob("*.patch")):
            continue  # empty placeholder dir
        if not any(directory.name in slug or slug in directory.name for slug in allowed):
            errors.append(
                f"patches/upstream/{directory.name}/ has patches but no port/patched-base row "
                f"in docs/THIRD_PARTY.md")
    return errors


def main() -> int:
    rows = parse_inventory()
    errors = check(rows) + check_pin(rows) + check_upstream_patches(rows)
    if errors:
        print("provenance check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(f"provenance OK: {len(rows)} dependencies, {len(list(NOTICES.glob('*.txt')))} notices")
    return 0


def _selftest() -> None:
    """Smallest thing that fails if the rules break."""
    good = [{"project": "Chromium", "repository": "https://x", "version": "151.0.7922.173",
             "license": "BSD-3-Clause", "mode": "patched-base", "notice": "chromium.txt"}]
    assert check_pin(good) == []
    bad_gpl = dict(good[0], project="uBO", license="GPL-3.0-or-later", mode="vendored",
                   notice="ublock-origin.txt")
    assert any("relicense" in e for e in check([bad_gpl])), "GPL rule not enforced"
    unpinned = dict(good[0], project="X", version="master", notice="chromium.txt")
    assert any("not pinned" in e for e in check([unpinned])), "pin rule not enforced"
    print("selftest OK")


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        _selftest()
        raise SystemExit(0)
    raise SystemExit(main())
