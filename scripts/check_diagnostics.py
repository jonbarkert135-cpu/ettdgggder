#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Gate: debug logs stay logs, crashes stay local, errors stay localized.

    python3 scripts/check_diagnostics.py
    python3 scripts/check_diagnostics.py --selftest

Roadmap items 79, 80 and 81 are three promises that decay in the same way: the
code is written correctly once, and then a later change adds an upload path, a
crash field, or an English string on an error page, and nothing notices. This
gate is what notices.

  79  the diagnostics tree contains no way to send anything anywhere, and
      logging is off by default in the code, not only in the documentation;
  81  crash upload consent defaults to "never", the report builder keeps a
      whitelist, and the field names that must never be collected are refused;
  80  every error code carries a title and an action, and both exist in every
      ship locale -- the error path is where hardcoded English survives longest.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DIAG = ROOT / "src_overrides" / "bedrock" / "diagnostics"
ERRORS = ROOT / "src_overrides" / "bedrock" / "errors"
CATALOG = ROOT / "src_overrides" / "bedrock" / "ui" / "l10n" / "string_catalog.cc"
DIAG_DOC = ROOT / "docs" / "DIAGNOSTICS.md"
ERROR_DOC = ROOT / "docs" / "ERRORS.md"

LOCALES = ["kEnglish", "kUkrainian", "kRussian", "kGerman"]

# Ways code sends bytes off the machine. A debug log needs none of them.
NETWORK_SYMBOLS = [
    "SimpleURLLoader", "URLFetcher", "network::", "net::URLRequest", "socket(",
    "curl_easy", "HttpPost", "SendRequest", "UploadFile(", "PostReport(",
    "WinHttp", "sendto(",
    "connect(", "XMLHttpRequest", "fetch(",
]
# Data that must never be attached to a crash report, whatever the consent.
MUST_REFUSE = [
    "url", "referrer", "page_title", "cookies", "password", "form_data",
    "session_token", "clipboard", "history", "bookmarks", "profile_path",
    "search_query",
]

CODE_LINE = re.compile(r'\{ErrorCode::k(?P<code>\w+),\s*"(?P<string>[A-Z]{2}-[A-Z]+-\d{3})",\s*'
                       r"MessageId::k(?P<title>\w+),\s*MessageId::k(?P<action>\w+),",
                       re.S)
ENUM_MEMBER = re.compile(r"^\s{2}k(?P<name>\w+),\s*(?://.*)?$", re.M)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(line for line in text.splitlines()
                     if not line.lstrip().startswith("//"))


def check_no_network(errors: list[str]) -> None:
    for path in sorted(DIAG.glob("*")):
        if path.suffix not in {".h", ".cc"} or path.name.endswith("_test.cc"):
            continue
        code = strip_comments(path.read_text(encoding="utf-8"))
        for symbol in NETWORK_SYMBOLS:
            if symbol in code:
                errors.append(
                    f"diagnostics/{path.name}: contains {symbol!r} — a debug log "
                    "with a network symbol is telemetry (item 79)")


def check_log_defaults(errors: list[str]) -> None:
    header = (DIAG / "debug_log.h").read_text(encoding="utf-8")
    if "Level level_ = Level::kOff;" not in header:
        errors.append("debug_log.h: logging must default to Level::kOff")
    if "static constexpr bool kUploadSupported = false;" not in header:
        errors.append("debug_log.h: kUploadSupported must be declared false")
    if "bool file_sink_enabled_ = false;" not in header:
        errors.append("debug_log.h: the file sink must default to off")
    sinks = re.search(r"enum class Sink \{(?P<body>.*?)\}", header, re.S)
    if not sinks:
        errors.append("debug_log.h: no Sink enum")
    else:
        names = ENUM_MEMBER.findall(sinks.group("body"))
        if sorted(names) != ["MemoryRing", "ProfileFile"]:
            errors.append(
                f"debug_log.h: Sink has {names} — every sink must be on this machine")


def check_crash_defaults(errors: list[str]) -> None:
    header = (DIAG / "crash_report.h").read_text(encoding="utf-8")
    source = (DIAG / "crash_report.cc").read_text(encoding="utf-8")
    consent = re.search(r"enum class UploadConsent \{(?P<body>.*?)\}", header, re.S)
    if not consent:
        errors.append("crash_report.h: no UploadConsent enum")
    else:
        names = ENUM_MEMBER.findall(consent.group("body"))
        if not names or names[0] != "Never":
            errors.append(
                f"crash_report.h: UploadConsent starts with {names[:1]} — "
                "kNever must be first, and the default (item 81)")
    if "UploadConsent consent_ = UploadConsent::kNever;" not in header:
        errors.append("crash_report.h: consent must default to kNever")
    forbidden = source[source.find("ForbiddenFieldList"):]
    for field in MUST_REFUSE:
        if f'"{field}"' not in forbidden:
            errors.append(f"crash_report.cc: {field!r} is not in the refused list")
    allowed = source[source.find("AllowedFieldList"):source.find("ForbiddenFieldList")]
    for field in MUST_REFUSE:
        if f'"{field}"' in allowed:
            errors.append(f"crash_report.cc: {field!r} must never be whitelisted")


def check_errors_localized(errors: list[str]) -> None:
    source = (ERRORS / "error_catalog.cc").read_text(encoding="utf-8")
    header = (ERRORS / "error_catalog.h").read_text(encoding="utf-8")
    catalog = CATALOG.read_text(encoding="utf-8")

    entries = list(CODE_LINE.finditer(source))
    enum = re.search(r"enum class ErrorCode \{(?P<body>.*?)\}", header, re.S)
    declared = [name for name in ENUM_MEMBER.findall(enum.group("body"))
                if not name.startswith("MaxValue")] if enum else []
    if len(entries) != len(declared):
        errors.append(
            f"error_catalog: {len(declared)} codes declared, {len(entries)} presented")

    seen_strings = set()
    for entry in entries:
        code_string = entry.group("string")
        if code_string in seen_strings:
            errors.append(f"error_catalog: duplicate code string {code_string}")
        seen_strings.add(code_string)
        for role in ("title", "action"):
            message_id = "k" + entry.group(role)
            for locale in LOCALES:
                needle = f"{{Locale::{locale}, MessageId::{message_id},"
                if needle not in catalog:
                    errors.append(
                        f"{code_string}: {message_id} has no {locale[1:]} translation "
                        "(item 80: errors are localized like every other string)")


def check_docs(errors: list[str]) -> None:
    for doc, phrases in (
        (DIAG_DOC, ["off by default", "DEBUG LOG", "TELEMETRY", "never uploaded"]),
        (ERROR_DOC, ["meaningful", "actionable", "localized", "security-conscious"]),
    ):
        if not doc.is_file():
            errors.append(f"docs/{doc.name}: missing")
            continue
        text = doc.read_text(encoding="utf-8")
        for phrase in phrases:
            if phrase.lower() not in text.lower():
                errors.append(f"docs/{doc.name}: does not state {phrase!r}")

    if DIAG_DOC.is_file():
        # Every field the code may attach must be listed for the reader; a crash
        # report whose contents are documented only in the source is a promise.
        source = (DIAG / "crash_report.cc").read_text(encoding="utf-8")
        allowed_block = source[source.find("AllowedFieldList"):source.find("ForbiddenFieldList")]
        doc_text = DIAG_DOC.read_text(encoding="utf-8")
        for field in re.findall(r'"(\w+)",\s*//', allowed_block):
            if field not in doc_text:
                errors.append(f"docs/DIAGNOSTICS.md: crash field {field!r} undocumented")


def selftest() -> int:
    errors: list[str] = []
    assert not ENUM_MEMBER.findall("enum class X { kA = 1 };"), "inline enum ignored"
    match = CODE_LINE.search(
        '{ErrorCode::kFoo, "BR-XXX-001",\n MessageId::kBar, MessageId::kBaz,\n')
    assert match and match.group("string") == "BR-XXX-001", "entry parser works"
    text = strip_comments("// SimpleURLLoader in a comment\nint x = 1;\n")
    assert "SimpleURLLoader" not in text, "comments are not code"
    for field in ("cookies", "password", "profile_path"):
        assert field in MUST_REFUSE, f"{field} must be on the refusal list"
    assert not errors
    print("check_diagnostics selftest OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return selftest()

    errors: list[str] = []
    check_no_network(errors)
    check_log_defaults(errors)
    check_crash_defaults(errors)
    check_errors_localized(errors)
    check_docs(errors)
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1
    codes = len(CODE_LINE.findall((ERRORS / "error_catalog.cc").read_text(encoding="utf-8")))
    print(f"diagnostics OK: logging off by default, no network symbol in the "
          f"diagnostics tree, crash upload defaults to never with "
          f"{len(MUST_REFUSE)} field names refused, {codes} error codes "
          f"localized in {len(LOCALES)} locales")
    return 0


if __name__ == "__main__":
    sys.exit(main())
