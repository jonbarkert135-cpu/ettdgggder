#!/usr/bin/env python3
"""Every anti-fingerprinting surface must have a documented rationale.

Roadmap item 10 requires docs/privacy/fingerprinting/<surface>.md with the
attack vector, mitigation, compatibility impact, performance impact and test
cases. This gate makes that structural: add a Surface to the enum without a
doc, or drop a required section, and CI fails.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
POLICY = ROOT / "src_overrides/bedrock/privacy/fingerprinting/fingerprint_policy.cc"
DOCS = ROOT / "docs/privacy/fingerprinting"
SECTIONS = ("## Attack vector", "## Mitigation", "## Compatibility impact",
            "## Performance impact", "## Test cases")


def main() -> int:
    ids = re.findall(r'\{Surface::k\w+, "([a-z0-9-]+)"', POLICY.read_text())
    if not ids:
        print("no surfaces found in", POLICY, file=sys.stderr)
        return 1

    errors = []
    for surface in ids:
        doc = DOCS / f"{surface}.md"
        if not doc.exists():
            errors.append(f"missing {doc.relative_to(ROOT)}")
            continue
        text = doc.read_text()
        errors += [f"{surface}.md: no '{s}' section" for s in SECTIONS
                   if s not in text]

    documented = {p.stem for p in DOCS.glob("*.md")} - {"README"}
    errors += [f"{extra}.md documents an unknown surface"
               for extra in sorted(documented - set(ids))]

    for error in errors:
        print("FAIL:", error, file=sys.stderr)
    print(f"fingerprinting docs OK: {len(ids)} surfaces documented")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
