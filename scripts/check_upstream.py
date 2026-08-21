#!/usr/bin/env python3
"""Upstream sync and patch gate (roadmap items 66, 67, 69).

The release policy is only worth having if the written policy and the executed
policy are the same thing. So this gate ties three artefacts together:

  * `updater/release_policy.cc` — the deadlines and pipeline stages the code
    enforces;
  * `docs/UPSTREAM_SYNC.md` — the deadlines and pipeline a human reads;
  * `docs/PATCHES.md` + `scripts/upstream_sync.py` — the patch header fields
    the documentation demands and the tool actually checks.

A security deadline that is 72 hours in the code and "one week" in the document
is worse than no document: during the one incident where it matters, people will
follow the wrong one.

It also runs the patch header check, so a patch that lands without a Reason or a
Drop-When fails CI rather than review.

Usage: python3 scripts/check_upstream.py [--selftest]
"""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
POLICY = REPO / "src_overrides" / "bedrock" / "updater" / "release_policy.cc"
DOC = REPO / "docs" / "UPSTREAM_SYNC.md"
PATCH_DOC = REPO / "docs" / "PATCHES.md"
TOOL = REPO / "scripts" / "upstream_sync.py"

SEVERITIES = ["Critical", "High", "Medium", "Low"]
HOUR_NAMES = {72: "72 hours", 168: "7 days", 336: "14 days", 720: "30 days"}


def code_deadlines(text: str) -> dict[str, int]:
    """Severity -> hours, read from the switch in DeadlineHours()."""
    body = re.search(r"int DeadlineHours\(.*?\n\}", text, re.S)
    if not body:
        sys.exit("release_policy.cc: DeadlineHours() not found")
    deadlines: dict[str, int] = {}
    for case, hours in re.findall(
        r"case Severity::k(\w+):\s*\n\s*return (\d+);", body.group(0)
    ):
        deadlines[case] = int(hours)
    return deadlines


def code_stages(text: str) -> list[str]:
    body = re.search(r"const char\* StageName\(.*?\n\}", text, re.S)
    if not body:
        sys.exit("release_policy.cc: StageName() not found")
    names = re.findall(r'return "([^"]+)";', body.group(0))
    # The trailing "unknown stage" exists to satisfy the compiler, not the pipeline.
    return [name for name in names if name != "unknown stage"]


def doc_deadlines(text: str) -> dict[str, str]:
    rows = re.findall(r"\|\s*(Critical|High|Medium|Low)\s*\|\s*([^|]+?)\s*\|", text)
    return {severity: value for severity, value in rows}


def check(errors: list[str]) -> tuple[int, int]:
    policy = POLICY.read_text(encoding="utf-8")
    doc = DOC.read_text(encoding="utf-8")

    deadlines = code_deadlines(policy)
    for severity in SEVERITIES:
        if severity not in deadlines:
            errors.append(f"DeadlineHours() has no case for {severity}")
    ordered = [deadlines.get(name, 0) for name in SEVERITIES]
    if ordered != sorted(ordered):
        errors.append(f"deadlines are not ordered by severity: {dict(zip(SEVERITIES, ordered))}")
    if deadlines.get("Critical", 999) > 72:
        errors.append("a public critical bug may not have a deadline longer than 72 hours")

    documented = doc_deadlines(doc)
    for severity in SEVERITIES:
        hours = deadlines.get(severity)
        stated = documented.get(severity)
        if stated is None:
            errors.append(f"docs/UPSTREAM_SYNC.md has no deadline row for {severity}")
        elif hours is not None and stated.strip() != HOUR_NAMES.get(hours, ""):
            errors.append(
                f"{severity}: code says {hours}h ({HOUR_NAMES.get(hours, '?')}), "
                f"docs/UPSTREAM_SYNC.md says '{stated}'"
            )

    stages = code_stages(policy)
    for stage in stages:
        if stage.lower() not in doc.lower():
            errors.append(f"pipeline stage '{stage}' is in the code but not in UPSTREAM_SYNC.md")
    if len(stages) < 5:
        errors.append(f"only {len(stages)} pipeline stages named in the code")

    # The two stages an emergency may never drop, checked as text so the
    # exception cannot quietly grow.
    emergency = re.search(r"if \(emergency\) \{\s*return \{([^}]*)\}", policy, re.S)
    if not emergency:
        errors.append("MandatoryStages(): the emergency branch is gone")
    else:
        for required in ("kSecurityReview", "kPrivacyRegression"):
            if required not in emergency.group(1):
                errors.append(f"an emergency release must still run {required}")

    # Patch header fields: documented and enforced must be the same set.
    tool = TOOL.read_text(encoding="utf-8")
    required = set(
        re.findall(r'"([\w-]+)",', re.search(r"REQUIRED_FIELDS = \[(.*?)\]", tool, re.S).group(1))
    )
    patch_doc = PATCH_DOC.read_text(encoding="utf-8")
    for field in sorted(required):
        if field not in patch_doc:
            errors.append(f"patch header field {field} is enforced but not documented in PATCHES.md")

    result = subprocess.run(
        [sys.executable, str(TOOL), "--check-patches"], capture_output=True, text=True
    )
    if result.returncode != 0:
        errors.append("upstream_sync.py --check-patches failed:\n    " + result.stderr.strip())

    return len(deadlines), len(stages)


def main() -> int:
    if "--selftest" in sys.argv:
        assert code_deadlines(
            "int DeadlineHours(Severity s) {\n  switch (s) {\n"
            "    case Severity::kCritical:\n      return 72;\n  }\n}"
        ) == {"Critical": 72}
        assert doc_deadlines("| Critical | 72 hours |\n| High | 7 days |") == {
            "Critical": "72 hours",
            "High": "7 days",
        }
        probe: list[str] = []
        assert code_stages('const char* StageName(Stage s) {\n return "security review";\n}') == [
            "security review"
        ]
        del probe
        print("selftest OK")
        return 0

    errors: list[str] = []
    deadlines, stages = check(errors)
    if errors:
        print("upstream check FAILED:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(
        f"upstream OK: {deadlines} severity deadlines and {stages} pipeline stages match the docs, "
        f"patch headers valid"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
