#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Gate: nothing decides anything about a host name with a prefix or a suffix.

    python3 scripts/check_host_matching.py
    python3 scripts/check_host_matching.py --selftest

Recommendation 4 of `docs/security/AUDIT-2026-08-25.md`. Finding F1 was one line
-- `StartsWith(host, "10.")` -- that handed a silent HTTPS downgrade to anyone
who registered `10.example.com`. Fixing that line fixes one bug; the class comes
back the next time someone writes the obvious thing. So: string prefix, suffix
and substring operations on a host-shaped variable are an error anywhere except
`src_overrides/bedrock/privacy/network/host_match.{h,cc}`, which implements them
once, on parsed octets and at label boundaries, with the attacks as its tests.

A variable is host-shaped if its name is or ends in host, hostname, domain,
etld, sld or site. Flagged is *deciding* by prefix or suffix -- `compare()`,
`starts_with`, `ends_with`, the `rfind(x, 0)` prefix idiom, and any
`StartsWith`/`EndsWith`-style call on such a variable. Splitting a host into
labels (`find('.')`, `substr`) is parsing, not deciding, and is allowed: the
first version of this gate flagged it and produced seven false positives in
code that was doing nothing wrong, which is how a gate gets switched off.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE_ROOT = ROOT / "src_overrides" / "bedrock"
HELPER = "privacy/network/host_match"
# The helper itself, and tests: a test *must* be able to spell out the wire form
# it is attacking.
EXEMPT_SUFFIXES = ("_test.cc",)
EXEMPT_STEMS = (HELPER,)

HOSTISH = r"(?:\w+_)?(?:host|hostname|domain|etld|sld|site)\d*"
# `host.compare(...)`, `host.starts_with(...)`, `host.ends_with(...)`
METHOD = re.compile(
    rf"\b{HOSTISH}\b\s*\.\s*(compare|starts_with|ends_with)\s*\(",
    re.IGNORECASE,
)
# The `host.rfind(prefix, 0) == 0` prefix idiom, which is the same bug wearing
# a search function's name.
RFIND_PREFIX = re.compile(rf"\b{HOSTISH}\b\s*\.\s*rfind\s*\([^;]*,\s*0\s*\)",
                          re.IGNORECASE)
# `StartsWith(host, ...)`, `EndsWith(some_domain, ...)`, `HasPrefix(host, ...)`
HELPER_CALL = re.compile(
    rf"\b(StartsWith|EndsWith|HasPrefix|HasSuffix)\s*\(\s*{HOSTISH}\b",
    re.IGNORECASE,
)
COMMENT = re.compile(r"^\s*(//|\*|/\*)")


def offenders(text: str) -> list[tuple[int, str]]:
    """Line number and line for each host comparison that is not the helper's."""
    found = []
    for number, line in enumerate(text.splitlines(), start=1):
        if COMMENT.match(line):
            continue
        if METHOD.search(line) or RFIND_PREFIX.search(line) or HELPER_CALL.search(line):
            found.append((number, line.strip()))
    return found


def exempt(relative: str) -> bool:
    return relative.endswith(EXEMPT_SUFFIXES) or any(
        relative.startswith(stem) for stem in EXEMPT_STEMS
    )


def scan() -> list[str]:
    errors = []
    for path in sorted(SOURCE_ROOT.rglob("*.[ch]*")):
        if path.suffix not in (".cc", ".h"):
            continue
        relative = path.relative_to(SOURCE_ROOT).as_posix()
        if exempt(relative):
            continue
        for number, line in offenders(path.read_text(encoding="utf-8")):
            errors.append(
                f"{relative}:{number} compares a host by prefix/suffix "
                f"({line[:70]}) -- use bedrock/{HELPER}.h"
            )
    return errors


def selftest() -> int:
    bad = "  if (StartsWith(host, \"10.\")) {\n  return domain.compare(0, 4, x);\n"
    assert len(offenders(bad)) == 2, offenders(bad)
    assert offenders("  // StartsWith(host, \"10.\") was F1\n") == [], "comments are prose"
    assert offenders("  if (StartsWith(url, \"https://\")) {\n") == [], "URLs are not hosts"
    assert offenders("  text.substr(0, 4);\n") == [], "only host-shaped names count"
    assert offenders("  request_host.rfind(needle, 0) == 0;\n"), (
        "the rfind prefix idiom on a host-shaped name is the same bug"
    )
    assert offenders("  const size_t dot = host.rfind('.');\n") == [], (
        "splitting a host into labels is parsing, not deciding"
    )
    assert offenders("  return host.substr(0, dot);\n") == [], "so is taking a label"
    assert offenders("  if (net::IsOrSubdomainOf(host, domain)) {\n") == [], (
        "the shared helper is what the gate is asking for"
    )
    assert exempt(f"{HELPER}.cc") and exempt("privacy/network/https_policy_test.cc")
    assert not exempt("privacy/network/https_policy.cc")
    print("check_host_matching selftest OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    if parser.parse_args().selftest:
        return selftest()

    errors = scan()
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        print(
            "A host name is not a string with a prefix. Compare it with "
            f"bedrock/{HELPER}.h, which parses addresses and matches on label "
            "boundaries (audit finding F1)."
        )
        return 1
    print("host matching OK: every host comparison goes through "
          f"bedrock/{HELPER}.h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
