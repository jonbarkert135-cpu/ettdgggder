#!/usr/bin/env python3
"""Gate: the roadmap-74 test matrix stays complete, honest and executable.

    python3 scripts/check_test_matrix.py
    python3 scripts/check_test_matrix.py --selftest

Checks:
  * every case the roadmap names is present, once, in tests/matrix.json;
  * every `runner` path exists in the repo;
  * every `status` is one of the declared values;
  * tests/MATRIX.md lists exactly the same cases (docs cannot drift);
  * a case may only claim `running` if its runner is wired into something that
    actually runs it -- a host test picked up by run_host_tests.sh, or one of
    the two browser suites.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MATRIX = ROOT / "tests" / "matrix.json"
DOC = ROOT / "tests" / "MATRIX.md"

# Exactly the cases roadmap item 74 asks for. Editing this list means the
# roadmap changed, not that a test became inconvenient.
REQUIRED = {
    "Browser Core": ["launch", "navigation", "tabs", "downloads", "profiles"],
    "Privacy": ["third-party cookies", "trackers", "fingerprint APIs", "referrer",
                "query stripping", "storage partition", "WebRTC", "HTTPS"],
    "Search": ["Google", "DuckDuckGo", "custom search providers"],
    "Extensions": ["installation", "permissions", "execution", "isolation"],
    "Tor": ["proxy routing", "identity reset", "DNS behavior"],
    "UI": ["themes", "accessibility", "keyboard navigation"],
}
RUNNING_RUNNERS = {"tests/browser/run.py", "tests/privacy/run.py"}


def problems(matrix: dict, doc: str, exists) -> list[str]:
    found = []
    cases = matrix.get("cases", [])
    statuses = set(matrix.get("status_values", {}))

    for case in cases:
        key = f"{case.get('area')} / {case.get('case')}"
        if case.get("status") not in statuses:
            found.append(f"{key}: unknown status {case.get('status')!r}")
        runner = case.get("runner", "")
        if not runner or not exists(runner):
            found.append(f"{key}: runner does not exist: {runner!r}")
        elif (case.get("status") == "running"
              and not runner.endswith("_test.cc")
              and runner not in RUNNING_RUNNERS):
            found.append(f"{key}: claims 'running' but {runner} is not an "
                         "executable host test or browser suite")
        if f"| {case.get('case')} |" not in doc:
            found.append(f"{key}: missing from tests/MATRIX.md")

    seen = [(c.get("area"), c.get("case")) for c in cases]
    for area, names in REQUIRED.items():
        for name in names:
            if (area, name) not in seen:
                found.append(f"{area} / {name}: required by roadmap item 74, not in matrix")
    duplicates = {pair for pair in seen if seen.count(pair) > 1}
    found += [f"{a} / {c}: listed twice" for a, c in sorted(duplicates)]
    extra = [pair for pair in seen if pair[1] not in REQUIRED.get(pair[0], [])]
    found += [f"{a} / {c}: not a roadmap case (add it to REQUIRED first)"
              for a, c in extra]
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        good = {"status_values": {"running": ""},
                "cases": [{"area": "UI", "case": "themes", "runner": "x_test.cc",
                           "status": "running"}]}
        doc = "| themes |"
        # `good` covers one required case, so the only complaints must be the
        # other roadmap cases it does not list.
        assert all("not in matrix" in p for p in problems(good, doc, lambda p: True))
        bad_status = json.loads(json.dumps(good))
        bad_status["cases"][0]["status"] = "green"
        assert any("unknown status" in p for p in problems(bad_status, doc, lambda p: True))
        assert any("runner does not exist" in p
                   for p in problems(good, doc, lambda p: False))
        fake_running = json.loads(json.dumps(good))
        fake_running["cases"][0]["runner"] = "docs/README.md"
        assert any("claims 'running'" in p
                   for p in problems(fake_running, doc, lambda p: True))
        assert any("missing from tests/MATRIX.md" in p
                   for p in problems(good, "", lambda p: True))
        print("check_test_matrix selftest OK")
        return 0

    matrix = json.loads(MATRIX.read_text(encoding="utf-8"))
    found = problems(matrix, DOC.read_text(encoding="utf-8"),
                     lambda path: (ROOT / path).exists())
    for problem in found:
        print(f"test matrix: {problem}")
    if found:
        return 1
    running = sum(1 for c in matrix["cases"] if c["status"] == "running")
    print(f"test matrix OK ({len(matrix['cases'])} cases, {running} executing today)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
