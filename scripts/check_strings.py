#!/usr/bin/env python3
"""Localization gate (roadmap item 61).

Holds the string catalog to the three rules that make translation possible:

  * every ship locale has every message id — a locale is complete or it is not
    offered, because a half-translated privacy UI is worse than an English one;
  * placeholders match across locales of the same id — a translation may reorder
    {site} and {count}, but dropping one loses information and inventing one
    fails at format time;
  * a counted message carries every plural category its locale needs — Russian
    and Ukrainian have four, and `if (n == 1)` is not a plural rule.

It also checks that docs/LOCALIZATION.md lists exactly the locales the catalog
ships, so the documentation cannot promise a language nobody translated.

Usage: python3 scripts/check_strings.py [--selftest]
"""

from __future__ import annotations

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
CATALOG = REPO / "src_overrides" / "bedrock" / "ui" / "l10n" / "string_catalog.cc"
DOC = REPO / "docs" / "LOCALIZATION.md"

ENTRY = re.compile(
    r"\{Locale::k(?P<locale>\w+),\s*MessageId::k(?P<id>\w+),\s*"
    r"PluralCategory::k(?P<category>\w+),\s*"
    r'"(?P<text>(?:[^"\\]|\\.)*)",\s*(?P<counted>true|false)\}',
    re.S,
)
LOCALE_INFO = re.compile(r'\{Locale::k(?P<locale>\w+), "(?P<tag>[a-z]{2}-[A-Z]{2})"')
PLACEHOLDER = re.compile(r"\{(\w+)\}")

# Plural categories a locale needs, from CLDR. Kept here as well as in the C++
# so the gate is an independent check rather than a mirror of the code it tests.
REQUIRED_CATEGORIES = {
    "English": {"One", "Other"},
    "German": {"One", "Other"},
    "Russian": {"One", "Few", "Many", "Other"},
    "Ukrainian": {"One", "Few", "Many", "Other"},
}


def parse_entries(text: str) -> list[dict]:
    entries = []
    for match in ENTRY.finditer(text):
        entries.append(
            {
                "locale": match.group("locale"),
                "id": match.group("id"),
                "category": match.group("category"),
                "text": match.group("text"),
                "counted": match.group("counted") == "true",
            }
        )
    return entries


def check(entries: list[dict], errors: list[str]) -> tuple[int, int]:
    locales = sorted({entry["locale"] for entry in entries})
    source = [entry for entry in entries if entry["locale"] == "English"]
    source_ids = sorted({entry["id"] for entry in source})

    for locale in locales:
        translated = {entry["id"] for entry in entries if entry["locale"] == locale}
        for message_id in source_ids:
            if message_id not in translated:
                errors.append(f"{locale}: no translation for {message_id}")
        if locale not in REQUIRED_CATEGORIES:
            errors.append(f"{locale}: no plural categories declared for this locale")

    for message_id in source_ids:
        expected = sorted(
            set(
                PLACEHOLDER.findall(
                    next(entry["text"] for entry in source if entry["id"] == message_id)
                )
            )
        )
        for locale in locales:
            texts = [
                entry["text"]
                for entry in entries
                if entry["id"] == message_id and entry["locale"] == locale
            ]
            for text in texts:
                found = sorted(set(PLACEHOLDER.findall(text)))
                if found != expected:
                    errors.append(
                        f"{locale} {message_id}: placeholders {found} != English {expected}"
                    )

        counted = [
            entry
            for entry in entries
            if entry["id"] == message_id and entry["counted"]
        ]
        if not counted:
            continue
        for locale in locales:
            have = {
                entry["category"] for entry in counted if entry["locale"] == locale
            }
            missing = REQUIRED_CATEGORIES.get(locale, set()) - have
            if missing:
                errors.append(
                    f"{locale} {message_id}: missing plural {sorted(missing)}"
                )

    return len(locales), len(source_ids)


def check_docs(text: str, errors: list[str]) -> None:
    doc = DOC.read_text(encoding="utf-8")
    tags = {match.group("tag") for match in LOCALE_INFO.finditer(text)}
    for tag in sorted(tags):
        if f"`{tag}`" not in doc:
            errors.append(f"{tag}: shipped by the catalog but missing from docs/LOCALIZATION.md")
    for documented in set(re.findall(r"`([a-z]{2}-[A-Z]{2})`", doc)):
        if documented not in tags:
            errors.append(f"{documented}: documented but not in the catalog")


def main() -> int:
    if "--selftest" in sys.argv:
        broken = parse_entries(
            '{Locale::kEnglish, MessageId::kA, PluralCategory::kOther, "hi {who}", false},\n'
            '{Locale::kGerman, MessageId::kA, PluralCategory::kOther, "hallo {wer}", false},\n'
        )
        probe: list[str] = []
        check(broken, probe)
        assert any("placeholders" in error for error in probe), probe
        missing = parse_entries(
            '{Locale::kEnglish, MessageId::kA, PluralCategory::kOther, "hi", false},\n'
            '{Locale::kEnglish, MessageId::kB, PluralCategory::kOther, "yo", false},\n'
            '{Locale::kRussian, MessageId::kA, PluralCategory::kOther, "прив", false},\n'
        )
        probe = []
        check(missing, probe)
        assert any("no translation for B" in error for error in probe), probe
        print("selftest OK")
        return 0

    text = CATALOG.read_text(encoding="utf-8")
    entries = parse_entries(text)
    errors: list[str] = []
    if len(entries) < 40:
        errors.append(f"only parsed {len(entries)} catalog entries — the parser or the table broke")
    locales, ids = check(entries, errors)
    check_docs(text, errors)

    if errors:
        print("localization check FAILED:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(f"localization OK: {locales} locales complete, {ids} message ids, placeholders and plurals match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
