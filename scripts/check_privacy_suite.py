#!/usr/bin/env python3
"""Gate: the privacy regression suite stays complete and stays local.

    python3 scripts/check_privacy_suite.py
    python3 scripts/check_privacy_suite.py --selftest

Roadmap item 75 names thirteen things the suite must cover and says the suite
must use its own local fixtures. Both are enforced here:

  * every named scenario exists in tests/privacy/expectations.json, has a
    requirement sentence and at least one machine-checkable assertion;
  * no fixture references an off-machine URL -- a privacy test that loads a
    third-party script is measuring that third party's day, not the browser;
  * the recorded stock-Chromium baseline covers every scenario, so "Bedrock
    changes X" can always be compared against a measurement.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SUITE = ROOT / "tests" / "privacy"
FIXTURES = SUITE / "fixtures"
EXPECTATIONS = SUITE / "expectations.json"
BASELINE = SUITE / "baseline-chromium.json"

REQUIRED_SCENARIOS = [
    "canvas", "webgl", "navigator", "screen", "timezone", "language", "fonts",
    "client_hints", "webrtc", "storage_isolation", "cookies", "referrer",
    "tracking_params",
]
# Anything that is not loopback. `{A}`/`{B}` are the runner's local origins.
EXTERNAL_URL = re.compile(r"""(?:https?:)?//(?!127\.0\.0\.1|localhost)[a-z0-9]""",
                          re.IGNORECASE)


def scenario_problems(expectations: dict) -> list[str]:
    found = []
    by_id = {s.get("id"): s for s in expectations.get("scenarios", [])}
    for name in REQUIRED_SCENARIOS:
        scenario = by_id.get(name)
        if scenario is None:
            found.append(f"scenario {name!r} required by item 75 is missing")
            continue
        if len(scenario.get("requirement", "")) < 20:
            found.append(f"scenario {name!r} has no readable requirement sentence")
        if not scenario.get("checks"):
            found.append(f"scenario {name!r} asserts nothing")
    for name in by_id:
        if name not in REQUIRED_SCENARIOS:
            found.append(f"scenario {name!r} is not in the item-75 list")
    return found


def fixture_problems(texts: dict[str, str]) -> list[str]:
    return [f"{name} references an off-machine URL: {match.group(0)!r}"
            for name, text in sorted(texts.items())
            if (match := EXTERNAL_URL.search(text))]


def baseline_problems(baseline: dict, expectations: dict) -> list[str]:
    ids = [s["id"] for s in expectations.get("scenarios", [])]
    return [f"baseline has no measurement for {name!r} (re-record with --baseline)"
            for name in ids if name not in baseline]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        ok = {"scenarios": [{"id": name, "requirement": "x" * 25, "checks": {"a": 1}}
                            for name in REQUIRED_SCENARIOS]}
        assert scenario_problems(ok) == []
        missing = {"scenarios": ok["scenarios"][1:]}
        assert any("is missing" in p for p in scenario_problems(missing))
        empty = json.loads(json.dumps(ok))
        empty["scenarios"][0]["checks"] = {}
        assert any("asserts nothing" in p for p in scenario_problems(empty))
        assert fixture_problems({"a.html": "<script src='_harness.js'>"}) == []
        assert fixture_problems({"a.html": "fetch('http://127.0.0.1:8080/x')"}) == []
        assert fixture_problems({"a.html": "//cdn.example.com/x.js"})
        assert fixture_problems({"a.html": "https://tracker.test/pixel.gif"})
        assert baseline_problems({name: {} for name in REQUIRED_SCENARIOS}, ok) == []
        assert baseline_problems({}, ok)
        print("check_privacy_suite selftest OK")
        return 0

    expectations = json.loads(EXPECTATIONS.read_text(encoding="utf-8"))
    texts = {path.name: path.read_text(encoding="utf-8")
             for path in sorted(FIXTURES.iterdir()) if path.is_file()}
    found = scenario_problems(expectations) + fixture_problems(texts)
    if BASELINE.exists():
        found += baseline_problems(
            json.loads(BASELINE.read_text(encoding="utf-8")), expectations)
    else:
        found.append("tests/privacy/baseline-chromium.json is missing")

    for problem in found:
        print(f"privacy suite: {problem}")
    if found:
        return 1
    print(f"privacy suite OK ({len(REQUIRED_SCENARIOS)} scenarios, "
          f"{len(texts)} local fixtures, baseline recorded)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
