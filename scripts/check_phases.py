#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Gate: the phase table stays complete and stays honest.

    python3 scripts/check_phases.py
    python3 scripts/check_phases.py --selftest

Roadmap item 89 gives nineteen phases in order. `docs/PHASES.md` keeps them next
to the real state of the project, and this gate stops that table from drifting
into optimism:

  * all nineteen phases are present, numbered 0 to 18, in order;
  * every status comes from the fixed vocabulary -- `done`, `policy-landed`,
    `not-started`. "In progress" is not a status, because it never ends;
  * a phase that needs a Chromium build cannot be `done` while no build has
    happened. `build/ENFORCEMENT.md` is the evidence that one has: it is the same
    file `check_no_fake_features.py` requires before a feature may claim to be
    enforced, so the two gates cannot disagree.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOC = ROOT / "docs" / "PHASES.md"
ENFORCEMENT = ROOT / "build" / "ENFORCEMENT.md"

STATUSES = {"done", "policy-landed", "not-started"}
# Phases whose completion is only observable in a running browser.
NEEDS_BUILD = set(range(1, 17)) - {17}
ROW = re.compile(r"^\|\s*(?P<phase>\d{1,2})\s*\|\s*(?P<work>[^|]+?)\s*\|\s*`(?P<status>[a-z-]+)`\s*\|",
                 re.M)


def parse(text: str) -> list[dict]:
    return [{"phase": int(m.group("phase")), "work": m.group("work").strip(),
             "status": m.group("status")} for m in ROW.finditer(text)]


def check(rows: list[dict], build_happened: bool) -> list[str]:
    errors = []
    numbers = [row["phase"] for row in rows]
    if numbers != list(range(19)):
        errors.append(f"docs/PHASES.md: phases are {numbers}, expected 0..18 in order")
    for row in rows:
        if row["status"] not in STATUSES:
            errors.append(
                f"phase {row['phase']}: status {row['status']!r} is not one of "
                f"{sorted(STATUSES)}")
        if (row["phase"] in NEEDS_BUILD and row["status"] == "done"
                and not build_happened):
            errors.append(
                f"phase {row['phase']} ({row['work']}): claims done, but no Chromium build is "
                "recorded in build/ENFORCEMENT.md")
    return errors


def selftest() -> int:
    sample = ("| 0 | Repo | `done` | x |\n| 1 | Chromium build | `done` | y |\n")
    rows = parse(sample)
    assert len(rows) == 2 and rows[1]["status"] == "done", rows
    errors = check(rows, build_happened=False)
    assert any("no Chromium build is recorded" in error for error in errors), errors
    assert any("expected 0..18" in error for error in errors), errors
    assert not check(parse("| 1 | Chromium build | `not-started` | y |"),
                     build_happened=False)[1:], "an honest row passes its own check"
    print("check_phases selftest OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    if parser.parse_args().selftest:
        return selftest()

    rows = parse(DOC.read_text(encoding="utf-8"))
    errors = check(rows, build_happened=ENFORCEMENT.is_file())
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1
    counts = {status: sum(1 for row in rows if row["status"] == status) for status in STATUSES}
    print(f"phases OK: 19 phases, {counts['done']} done, "
          f"{counts['policy-landed']} policy-landed (no Chromium build yet), "
          f"{counts['not-started']} not started")
    return 0


if __name__ == "__main__":
    sys.exit(main())
