#!/usr/bin/env python3
"""Fail if the licensing record is incomplete. Run: python3 scripts/check_provenance.py

Enforces docs/LICENSING.md section 7:
  1. inventory row <-> notice file are 1:1
  2. versions are pinned (except reuse mode 'reimplement', which ships no code)
  3. GPL-family licenses may only be 'separate-artifact' or 'not-used'
  4. build/chromium.pin is well formed and matches the Chromium inventory row
  5. every patch under patches/upstream/<project>/ maps to a port/patched-base row
  6. docs/privacy/FILTER_LISTS.md: one licence row per list, defaults verified, none bundled
  7. zero-trust dependencies (item 77): every row is reviewed within the last
     year and carries a justification that is about need, not taste
  8. per-file provenance (item 91): docs/PROVENANCE.md records every file in the
     tree that came from a third party, with the seven fields item 91 asks for,
     and the record and the inventory mode cannot drift apart
"""

from __future__ import annotations

import datetime
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
INVENTORY = REPO / "docs" / "THIRD_PARTY.md"
NOTICES = REPO / "THIRD_PARTY_NOTICES"
PIN = REPO / "build" / "chromium.pin"
FILTER_LISTS = REPO / "docs" / "privacy" / "FILTER_LISTS.md"
PROVENANCE = REPO / "docs" / "PROVENANCE.md"

# Item 91: what a record must say about a piece of third-party material.
PROVENANCE_FIELDS = ["file", "project", "license", "version", "commit",
                     "upstream_file", "status", "attribution"]
MODIFICATION_STATUSES = {"verbatim", "modified", "data-snapshot"}
# Only these modes mean "their code/data is in our tree", so only these require
# -- and permit -- a per-file record.
CODE_IN_TREE_MODES = {"port", "vendored"}

# Item 77: a dependency must be justified by what it does for the product, not
# by taste or momentum. These are the words that show up when it is neither.
NON_JUSTIFICATIONS = (
    "prettier", "nicer", "looks better", "convenient", "convenience",
    "everyone uses", "popular", "modern", "industry standard", "why not",
    "saves time", "easier",
)
REVIEW_MAX_AGE_DAYS = 365

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
        if len(cells) != 8 or cells[0] in ("Project",) or set(cells[0]) <= set("- "):
            continue
        rows.append(dict(zip(
            ["project", "repository", "version", "license", "mode", "notice",
             "reviewed", "justification"], cells)))
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


def check_zero_trust(rows: list[dict[str, str]]) -> list[str]:
    """Item 77: identified, pinned, licensed, reviewed, justified.

    The first three are checked elsewhere in this file. These are the two that
    rot silently: a review nobody repeats, and a justification nobody wrote.
    """
    errors: list[str] = []
    today = datetime.date.today()
    for row in rows:
        project = row["project"]

        reviewed = row.get("reviewed", "")
        try:
            date = datetime.date.fromisoformat(reviewed)
        except ValueError:
            errors.append(f"{project}: 'Reviewed' must be an ISO date, got {reviewed!r}")
        else:
            if date > today:
                errors.append(f"{project}: reviewed in the future ({reviewed})")
            elif (today - date).days > REVIEW_MAX_AGE_DAYS:
                errors.append(
                    f"{project}: last reviewed {reviewed}, more than "
                    f"{REVIEW_MAX_AGE_DAYS} days ago — re-check the version, licence and whether "
                    f"it is still needed, then update the row"
                )

        justification = row.get("justification", "")
        if len(justification) < 20:
            errors.append(
                f"{project}: 'Justification' is empty or too short to be one ({justification!r})"
            )
        lowered = justification.lower()
        for phrase in NON_JUSTIFICATIONS:
            if phrase in lowered:
                errors.append(
                    f"{project}: {phrase!r} is not a justification — say what breaks without it "
                    f"(docs/DEPENDENCIES.md)"
                )
    return errors


def parse_provenance(text: str) -> list[dict[str, str]]:
    block = re.search(r"<!-- BEGIN PROVENANCE -->(.*?)<!-- END PROVENANCE -->", text, re.S)
    if not block:
        sys.exit("PROVENANCE.md: PROVENANCE markers not found")
    records = []
    for line in block.group(1).strip().splitlines():
        cells = [c.strip().strip("`") for c in line.strip().strip("|").split("|")]
        if len(cells) != 8 or cells[0].startswith("File in") or set(cells[0]) <= set("- "):
            continue
        records.append(dict(zip(PROVENANCE_FIELDS, cells)))
    return records


def check_provenance_records(rows: list[dict[str, str]]) -> list[str]:
    """Item 91: traceable in both directions, or it is not traceable at all."""
    errors: list[str] = []
    records = parse_provenance(PROVENANCE.read_text())
    by_project = {row["project"]: row for row in rows}

    for record in records:
        where = record["file"]
        if not (REPO / record["file"]).is_file():
            errors.append(f"PROVENANCE.md: {where} does not exist in the tree")
        row = by_project.get(record["project"])
        if row is None:
            errors.append(f"PROVENANCE.md: {where} names {record['project']!r}, "
                          "which is not in the THIRD_PARTY.md inventory")
        else:
            if row["license"] != record["license"]:
                errors.append(
                    f"PROVENANCE.md: {where} says {record['license']!r} but the "
                    f"inventory says {row['license']!r} for {record['project']}")
            if row["mode"] not in CODE_IN_TREE_MODES:
                errors.append(
                    f"PROVENANCE.md: {where} comes from {record['project']}, whose "
                    f"reuse mode is {row['mode']!r} -- a file in the tree means "
                    "mode 'port' or 'vendored'")
        if record["status"] not in MODIFICATION_STATUSES:
            errors.append(f"PROVENANCE.md: {where} has modification status "
                          f"{record['status']!r} (allowed: {sorted(MODIFICATION_STATUSES)})")
        for field in ("commit", "upstream_file", "attribution", "version"):
            if not record[field] or record[field] in ("-", "?", "unknown"):
                errors.append(f"PROVENANCE.md: {where} does not record its {field}")

    recorded_projects = {record["project"] for record in records}
    for row in rows:
        if row["mode"] in CODE_IN_TREE_MODES and row["project"] not in recorded_projects:
            errors.append(
                f"{row['project']}: reuse mode {row['mode']!r} claims their material is in "
                "the tree, but PROVENANCE.md has no record of a single file -- either add "
                "the record or set the mode to 'reimplement'/'not-used'")

    # The honesty direction: a file that says where it came from must be recorded.
    recorded_files = {record["file"] for record in records}
    for path in sorted((REPO / "src_overrides").rglob("*")):
        if path.suffix not in (".cc", ".h", ".js", ".html", ".json"):
            continue
        head = path.read_text(errors="replace")[:4000]
        if "Derived-from:" in head:
            relative = path.relative_to(REPO).as_posix()
            if relative not in recorded_files:
                errors.append(f"{relative}: declares Derived-from: but has no "
                              "PROVENANCE.md record")
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


def check_filter_lists() -> list[str]:
    """Filter lists are data with their own licences, one per list (item 52).

    A list is only a default subscription once someone verified its licence, and
    no list file is ever committed: shipping one distributes it under its terms.
    """
    errors: list[str] = []
    text = FILTER_LISTS.read_text()
    block = re.search(r"<!-- BEGIN FILTER LISTS -->(.*?)<!-- END FILTER LISTS -->", text, re.S)
    if not block:
        return ["docs/privacy/FILTER_LISTS.md: FILTER LISTS markers not found"]
    rows = 0
    for line in block.group(1).strip().splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) != 5 or cells[0] == "List" or set(cells[0]) <= set("- "):
            continue
        rows += 1
        name, author, licence, verified, default = cells
        if not author or not licence:
            errors.append(f"filter list {name!r}: author and licence are required")
        if default.lower().startswith("yes") and verified.lower() == "unverified":
            errors.append(
                f"filter list {name!r}: default subscription with an unverified licence")
    if not rows:
        errors.append("docs/privacy/FILTER_LISTS.md: inventory is empty")

    for path in (REPO / "src_overrides").rglob("*.txt"):
        head = path.read_text(errors="replace")[:2000]
        if any(marker in head for marker in ("||", "##", "@@")):
            errors.append(f"{path.relative_to(REPO)}: filter list data is never committed")
    return errors


def main() -> int:
    rows = parse_inventory()
    errors = (check(rows) + check_pin(rows) + check_upstream_patches(rows)
              + check_zero_trust(rows) + check_filter_lists()
              + check_provenance_records(rows))
    if errors:
        print("provenance check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    records = parse_provenance(PROVENANCE.read_text())
    print(f"provenance OK: {len(rows)} dependencies, "
          f"{len(list(NOTICES.glob('*.txt')))} notices, "
          f"{len(records)} per-file records (item 91)")
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
    assert check_filter_lists() == [], "filter list inventory should be clean"

    # Item 91, both directions.
    claiming = [{"project": "brave-core", "repository": "https://x", "version": "v1",
                 "license": "MPL-2.0", "mode": "port", "notice": "brave-core.txt",
                 "reviewed": "2026-08-25", "justification": "x"}]
    assert any("no record of a single file" in e
               for e in check_provenance_records(claiming)), "a port with no record must fail"
    sample = parse_provenance(
        "<!-- BEGIN PROVENANCE -->\n"
        "| File in this tree | Source project | Licence | Version | Commit / snapshot |"
        " Upstream file | Modification status | Attribution |\n"
        "|---|---|---|---|---|---|---|---|\n"
        "| `docs/PROVENANCE.md` | PrivacyTools.io | VERNAM License | v1 | snapshot-1 |"
        " https://u/ | verbatim | credit link |\n"
        "<!-- END PROVENANCE -->\n")
    assert len(sample) == 1 and sample[0]["status"] == "verbatim", sample
    assert check_provenance_records([]) , "a record naming no inventory project must fail"
    print("selftest OK")


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        _selftest()
        raise SystemExit(0)
    raise SystemExit(main())
