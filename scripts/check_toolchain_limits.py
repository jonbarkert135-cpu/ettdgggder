#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Gate: overlay code stays inside what Chromium's toolchain accepts.

    python3 scripts/check_toolchain_limits.py
    python3 scripts/check_toolchain_limits.py --selftest

The host tests compile the overlay with `g++` and libstdc++, which is far more
forgiving than the compiler that matters: inside the Chromium tree the same files
are built by Chromium's clang with `-fno-exceptions` and with the C++20
standard-library modules, where a standard symbol is only visible if its own
header is imported. The gap between the two is expensive -- it is only ever
discovered by a 12-hour in-tree build failing on its last steps, which is exactly
how `std::abs` in `privacy/network/request_headers.cc` was found (PR #53).

So the constraints documented in docs/BUILD.md are checked mechanically here,
before a push, without a Chromium checkout:

  1. `-fno-exceptions`: no `std::stoi`/`std::stod`/`std::stol...`, no `try`/
     `catch`, no `throw`. Parse with `strtol`/`strtod` and return a failure
     value.
  2. C++ modules: no `<cstdlib>` numeric helper (`std::abs`, `std::labs`,
     `std::div`) -- handle the sign or the division by hand. These are the
     symbols clang refuses without a module import even when the header is
     included, and they all have a two-line replacement.

The scan is over the overlay C++ that Chromium's clang actually compiles:
`src_overrides/`, minus `*_test.cc`. The host tests are never part of the
in-tree build (`scripts/gen_build_gn.py` keeps them out of `BUILD.gn`) -- they
are built by `g++` alone, where `std::abs` and exceptions are available, so
holding them to a constraint that cannot reach them would only cost churn.
`fuzz/` *is* scanned: libFuzzer harnesses are built by the Chromium toolchain.
`patches/` is Chromium's own code and is not ours to constrain.

Comments and strings are excluded: naming a banned symbol in an explanation of
why it is banned must stay legal, or the fix cannot document itself.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OVERLAY = ROOT / "src_overrides"
DOC = ROOT / "docs" / "BUILD.md"
CPP_SUFFIXES = {".cc", ".h", ".cpp", ".hpp"}

# Each rule: (compiled pattern, why it fails, what to do instead).
RULES: list[tuple[re.Pattern[str], str, str]] = [
    (re.compile(r"\bstd::sto[a-z]+\s*\("),
     "std::sto* throws, and the Chromium build is -fno-exceptions",
     "use strtol/strtod and return a failure value"),
    (re.compile(r"(?:^|[^\w:])(?:try\s*\{|catch\s*\(|throw\b)"),
     "exceptions are disabled in the Chromium build (-fno-exceptions)",
     "return a status or a sentinel instead"),
    (re.compile(r"\bstd::(?:abs|labs|llabs|div|ldiv|lldiv)\s*\("),
     "<cstdlib> numeric helpers are not visible in the C++ modules build",
     "handle the sign or the quotient by hand (see OneDecimal())"),
]


def strip_comments_and_strings(text: str) -> str:
    """Blank out //, /* */, "..." and '...' so prose cannot trip a rule.

    Characters are replaced by spaces rather than removed, so a line number
    computed from the result still matches the file.
    """
    out: list[str] = []
    i, n = 0, len(text)
    while i < n:
        two = text[i:i + 2]
        if two == "//":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
            continue
        if two == "/*":
            while i < n and text[i:i + 2] != "*/":
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append("  ")
            i += 2
            continue
        if text[i] in "\"'":
            quote = text[i]
            out.append(" ")
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    out.append(" ")
                    i += 1
                if i < n:
                    out.append("\n" if text[i] == "\n" else " ")
                    i += 1
            out.append(" ")
            i += 1
            continue
        out.append(text[i])
        i += 1
    return "".join(out)


def scan_text(rel: str, text: str, errors: list[str]) -> None:
    code = strip_comments_and_strings(text)
    for pattern, why, instead in RULES:
        for match in pattern.finditer(code):
            line = code.count("\n", 0, match.start()) + 1
            errors.append(f"{rel}:{line}: {match.group().strip()} — {why}; {instead}")


def overlay_files() -> list[pathlib.Path]:
    if not OVERLAY.is_dir():
        return []
    return [p for p in sorted(OVERLAY.rglob("*"))
            if p.is_file() and p.suffix in CPP_SUFFIXES
            and not p.name.endswith("_test.cc")]


def check_doc(errors: list[str]) -> None:
    """The rule has to be written where a contributor will meet it."""
    if not DOC.is_file():
        errors.append("docs/BUILD.md: missing")
        return
    text = DOC.read_text(encoding="utf-8")
    for phrase in ("-fno-exceptions", "std::abs"):
        if phrase not in text:
            errors.append(f"docs/BUILD.md: does not mention {phrase}")


def selftest() -> int:
    errors: list[str] = []
    scan_text("fake.cc", "int n = std::stoi(s);\n", errors)
    assert errors, "std::stoi must be caught"
    errors = []
    scan_text("fake.cc", "return std::to_string(std::abs(scaled % 10));\n", errors)
    assert errors and "modules build" in errors[0], f"std::abs must be caught, got {errors}"
    errors = []
    scan_text("fake.cc", "try { f(); } catch (const E& e) { g(); }\n", errors)
    assert len(errors) >= 2, f"try and catch must both fire, got {errors}"
    errors = []
    scan_text("ok.cc",
              "// std::abs() is not visible here, so the sign is handled by hand.\n"
              "/* neither std::stoi nor try/catch may appear */\n"
              'const char* kMsg = "std::abs and catch (";\n'
              "long v = strtol(s.c_str(), &end, 10);\n"
              "if (v < 0) { v = -v; }\n", errors)
    assert not errors, f"prose and strings must pass, got {errors}"
    errors = []
    scan_text("ok.cc", "int catcher(int catch_all) { return catch_all; }\n", errors)
    assert not errors, f"an identifier containing 'catch' must pass, got {errors}"
    print("check_toolchain_limits selftest OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return selftest()

    errors: list[str] = []
    files = overlay_files()
    for path in files:
        scan_text(path.relative_to(ROOT).as_posix(),
                  path.read_text(encoding="utf-8", errors="replace"), errors)
    check_doc(errors)
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1
    print(f"toolchain limits OK: {len(files)} in-tree overlay C++ files, "
          "no exceptions, no <cstdlib> numeric helpers")
    return 0


if __name__ == "__main__":
    sys.exit(main())
