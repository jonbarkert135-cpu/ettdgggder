#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Gate: every privacy feature explains itself, including its limits.

    python3 scripts/check_transparency.py
    python3 scripts/check_transparency.py --write      # regenerate the docs
    python3 scripts/check_transparency.py --selftest

Roadmap items 82 and 85. The disclosure table in
`src_overrides/bedrock/settings/knowledge/feature_disclosure.cc` is the single
source: the host test checks it against the feature registry, and this gate
renders it into two documents and fails when they drift.

  * `docs/privacy/FEATURES.md` -- item 82's four statements per feature. The one
    that matters is "what it cannot protect": a protection with no stated limit
    is one nobody has thought about hard enough, and this gate rejects an empty
    or evasive answer.
  * `docs/privacy/TRADEOFFS.md` -- item 85's five scores per feature, plus the
    verdict on whether the trade-off is good enough to be a default.

Writing the docs from the code rather than beside it means the published
description of a protection cannot be nicer than the one in the source.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TABLE = ROOT / "src_overrides" / "bedrock" / "settings" / "knowledge" / "feature_disclosure.cc"
FEATURES_DOC = ROOT / "docs" / "privacy" / "FEATURES.md"
TRADEOFFS_DOC = ROOT / "docs" / "privacy" / "TRADEOFFS.md"

EVASIVE = {"", "nothing", "nothing.", "n/a", "none", "none.", "unknown", "tbd"}
SCORE_NAMES = ["privacy gain", "security gain", "compatibility loss",
               "performance cost", "complexity"]


def split_top_level(chunk: str) -> list[str]:
    """Split a C++ initialiser body on commas that are not inside braces or strings."""
    parts, depth, in_string, escaped, current = [], 0, False, False, ""
    for char in chunk:
        if in_string:
            current += char
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
            current += char
        elif char in "{":
            depth += 1
            current += char
        elif char == "}":
            depth -= 1
            current += char
        elif char == "," and depth == 0:
            parts.append(current)
            current = ""
        else:
            current += char
    if current.strip():
        parts.append(current)
    return parts


def literal(text: str) -> str:
    """Join adjacent C++ string literals into the string the compiler sees."""
    return "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', text)).replace('\\"', '"')


def parse_table() -> list[dict]:
    source = TABLE.read_text(encoding="utf-8")
    body = source[source.index("static const std::vector<Disclosure> table"):]
    rows = []
    for start in [m.start() for m in re.finditer(r"\{Feature::", body)]:
        # Walk to the matching brace: rows differ in shape (an optional
        # exception string follows the trade-off), so counting is safer than a
        # pattern that guesses where a row ends.
        depth, index, in_string, escaped = 0, start, False, False
        while index < len(body):
            char = body[index]
            if in_string:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    in_string = False
            elif char == '"':
                in_string = True
            elif char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    break
            index += 1
        row_text = body[start + 1:index]
        name = row_text.split(",")[0].strip().removeprefix("Feature::")
        rest = row_text[row_text.index(",") + 1:]
        fields = split_top_level(rest)
        # id, how, protects, cannot, compatibility, {tradeoff...
        scores_text = fields[5]
        numbers = [int(value) for value in re.findall(r"-?\d+", scores_text.split('"')[0])]
        rows.append({
            "feature": name,
            "id": literal(fields[0]),
            "how": literal(fields[1]),
            "protects": literal(fields[2]),
            "cannot": literal(fields[3]),
            "compat": literal(fields[4]),
            "scores": numbers,
            "note": literal(scores_text),
            "exception": literal(fields[6]) if len(fields) > 6 else "",
        })
    return rows


def defaultable(scores: list[int]) -> bool:
    gain = max(scores[0], scores[1])
    cost = scores[2] + scores[3]
    if gain < 1 or scores[2] >= 3:
        return False
    if cost == 0:
        return True
    return gain >= 2 and cost <= gain


def validate(rows: list[dict]) -> list[str]:
    errors = []
    if not rows:
        errors.append("feature_disclosure.cc: no rows parsed")
    seen = set()
    for row in rows:
        name = row["id"] or row["feature"]
        if name in seen:
            errors.append(f"{name}: duplicate id")
        seen.add(name)
        if row["cannot"].strip().lower() in EVASIVE:
            errors.append(f"{name}: 'what it cannot protect' is evasive (item 82)")
        for field in ("how", "protects", "cannot", "compat"):
            if len(row[field]) < 20:
                errors.append(f"{name}: {field} is too short to be a real statement")
        if len(row["scores"]) != 5:
            errors.append(f"{name}: expected 5 scores, got {len(row['scores'])}")
            continue
        for score, label in zip(row["scores"], SCORE_NAMES):
            if not 0 <= score <= 3:
                errors.append(f"{name}: {label} out of range ({score})")
        if 3 in row["scores"] and len(row["note"]) < 30:
            errors.append(f"{name}: a score of 3 needs an explanation (item 85)")
    return errors


def render_features(rows: list[dict]) -> str:
    out = ["# Privacy features, in full",
           "",
           "**Roadmap item 82.** Generated from the disclosure table in",
           "[`src_overrides/bedrock/settings/knowledge/feature_disclosure.cc`]"
           "(../../src_overrides/bedrock/settings/knowledge/feature_disclosure.cc)",
           "by `scripts/check_transparency.py --write`; the gate fails if this file drifts from it.",
           "",
           'A checkbox labelled "Ultimate Privacy" is a marketing claim wearing a control\'s clothes.',
           "Every protection here states four things, and the settings UI shows them next to the",
           "control rather than in a help centre nobody opens. The third one — what it *cannot* do —",
           "is the one worth reading.",
           ""]
    for row in rows:
        out += [f"## {row['id']}",
                "",
                f"**How it works.** {row['how']}",
                "",
                f"**What it protects.** {row['protects']}",
                "",
                f"**What it cannot protect.** {row['cannot']}",
                "",
                f"**Compatibility impact.** {row['compat']}",
                ""]
    return "\n".join(out)


def render_tradeoffs(rows: list[dict]) -> str:
    out = ["# Privacy versus usability: the scoring table",
           "",
           "**Roadmap item 85.** Generated from the same table as",
           "[`FEATURES.md`](FEATURES.md) by `scripts/check_transparency.py --write`.",
           "",
           "Never optimise privacy blindly. Before a protection is switched on by default it is",
           "scored on five axes — 0 none, 1 low, 2 medium, 3 high — and the score lives in the",
           "source next to the feature, not in a review thread that scrolls away.",
           "",
           "*Default-able* means the gain is at least medium and does not cost more than it buys;",
           "a compatibility loss of 3 is never a default, whatever it protects. The column is",
           "computed, not typed: `IsDefaultable()` in the same file, tested by",
           "`feature_disclosure_test.cc`.",
           "",
           "| Feature | Privacy | Security | Compat. loss | Perf. cost | Complexity | Default-able |",
           "| --- | --- | --- | --- | --- | --- | --- |"]
    for row in rows:
        privacy, security, compat, perf, complexity = row["scores"]
        verdict = "yes" if defaultable(row["scores"]) else "**no — opt-in**"
        out.append(f"| `{row['id']}` | {privacy} | {security} | {compat} | {perf} | "
                   f"{complexity} | {verdict} |")
    out += ["", "## Shipped on against the score", "",
            "Where the arithmetic says opt-in and the feature is on anyway, the argument is",
            "recorded in the source and reproduced here. The host test fails if one is missing.",
            ""]
    exceptions = [row for row in rows if row["exception"]]
    for row in exceptions:
        out.append(f"* **`{row['id']}`** — {row['exception']}")
    if not exceptions:
        out.append("*None: every default follows its own score.*")
    out += ["", "## Where a score of 3 is explained", ""]
    for row in rows:
        if 3 in row["scores"] and row["note"]:
            out.append(f"* **`{row['id']}`** — {row['note']}")
    out += ["",
            "## How to read a 'no'",
            "",
            "A `no` is not a rejection of the feature. It means the protection ships available but",
            "off, reachable from the privacy axis (item 83) by a user who has decided the breakage",
            "is worth it. Blocking every third-party request is the clearest case: the privacy gain",
            "is the highest in the table, and so is the compatibility loss.",
            ""]
    return "\n".join(out)


def selftest() -> int:
    chunk = '"an_id",\n "how it " "works",\n "protects",\n "cannot",\n "compat",\n {1, 2, 3, 0, 1,\n "note"}'
    fields = split_top_level(chunk)
    assert len(fields) == 6, fields
    assert literal(fields[1]) == "how it works", literal(fields[1])
    assert defaultable([3, 1, 1, 0, 2]) is True
    assert defaultable([3, 2, 3, 0, 1]) is False, "a loss of 3 is never a default"
    assert defaultable([1, 1, 0, 0, 0]) is True, "a free protection is on"
    assert defaultable([1, 0, 1, 0, 0]) is False
    bad = validate([{"id": "x", "feature": "kX", "how": "a" * 40, "protects": "b" * 40,
                     "cannot": "nothing.", "compat": "c" * 40, "scores": [1, 1, 1, 1, 1],
                     "note": ""}])
    assert any("evasive" in error for error in bad), bad
    rows = parse_table()
    assert len(rows) >= 25, f"parser lost rows: {len(rows)}"
    assert any(row["exception"] for row in rows), "exception field must parse"
    print(f"check_transparency selftest OK ({len(rows)} rows parsed)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true", help="regenerate the documents")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return selftest()

    rows = parse_table()
    errors = validate(rows)
    documents = {FEATURES_DOC: render_features(rows), TRADEOFFS_DOC: render_tradeoffs(rows)}
    if args.write:
        for path, text in documents.items():
            path.write_text(text, encoding="utf-8")
            print(f"wrote {path.relative_to(ROOT)}")
        return 1 if errors else 0

    for path, text in documents.items():
        if not path.is_file():
            errors.append(f"{path.relative_to(ROOT)}: missing — run --write")
        elif path.read_text(encoding="utf-8") != text:
            errors.append(f"{path.relative_to(ROOT)}: stale — run "
                          "scripts/check_transparency.py --write")
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1
    opt_in = sum(1 for row in rows if not defaultable(row["scores"]))
    print(f"transparency OK: {len(rows)} features each state how they work, what they "
          f"protect, what they cannot protect and what they break; {opt_in} score as "
          "opt-in rather than default")
    return 0


if __name__ == "__main__":
    sys.exit(main())
