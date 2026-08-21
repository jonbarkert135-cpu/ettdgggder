#!/usr/bin/env python3
"""Branding gate (roadmap item 65).

Bedrock ships its own identity. Three ways that quietly stops being true, all
checked here:

  * **Another vendor's name reaches the interface.** Not by malice — a mockup
    label, a placeholder string, an icon file copied "for now". The engine is
    Chromium and prose may say so, but no browser vendor's brand belongs in a
    user-visible string, and nothing may claim to *be* Tor Browser (item 51).
  * **The brand documentation and the assets drift.** A documented logo file
    that does not exist, or an asset nobody documents and everyone is afraid to
    delete.
  * **Colour values get restated.** The palette lives in design-tokens.json;
    when BRAND.md names an accent, it must be the same accent.

Usage: python3 scripts/check_branding.py [--selftest]
"""

from __future__ import annotations

import json
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
BRANDING = REPO / "branding"
TOKENS = BRANDING / "design-tokens.json"
DOC = REPO / "docs" / "BRAND.md"
CATALOG = REPO / "src_overrides" / "bedrock" / "ui" / "l10n" / "string_catalog.cc"
MOCKUPS = REPO / "docs" / "design" / "mockups"

# Vendor brands that must not appear in anything a user sees. "Chromium" is
# absent on purpose: it is the engine, stated in prose, and honesty about it is
# an obligation (docs/LICENSING.md), not a branding violation.
FORBIDDEN = [
    "firefox",
    "brave",
    "google",
    "chrome",
    "mozilla",
    "safari",
    "microsoft edge",
    "apple",
    "microsoft",
    "tor browser",
    "onion router",
]

MESSAGE_TEXT = re.compile(
    r"PluralCategory::k\w+,\s*\n?\s*\"((?:[^\"\\]|\\.)*)\"", re.S
)


def visible_text(html: str) -> str:
    """Everything a reader sees: no comments, no CSS, no tags, no attributes."""
    html = re.sub(r"<!--.*?-->", " ", html, flags=re.S)
    html = re.sub(r"<(style|script)\b.*?</\1>", " ", html, flags=re.S | re.I)
    html = re.sub(r"<[^>]+>", " ", html)
    return html


def find_forbidden(text: str) -> list[str]:
    lowered = text.lower()
    return [name for name in FORBIDDEN if re.search(rf"\b{re.escape(name)}\b", lowered)]


def check_strings(errors: list[str]) -> int:
    texts = MESSAGE_TEXT.findall(CATALOG.read_text(encoding="utf-8"))
    if len(texts) < 40:
        errors.append(f"only found {len(texts)} catalog strings — the parser broke")
    for text in texts:
        for name in find_forbidden(text):
            errors.append(f'string catalog: "{text}" contains the brand "{name}"')
    return len(texts)


def check_mockups(errors: list[str]) -> int:
    files = sorted(MOCKUPS.glob("*.html"))
    for path in files:
        for name in find_forbidden(visible_text(path.read_text(encoding="utf-8"))):
            errors.append(f"{path.relative_to(REPO)}: visible text contains the brand \"{name}\"")
    return len(files)


def check_assets(errors: list[str]) -> int:
    doc = DOC.read_text(encoding="utf-8")
    on_disk = {
        path.name
        for path in BRANDING.iterdir()
        if path.is_file() and path.suffix in {".svg", ".png", ".ico"}
    }
    documented = set(re.findall(r"branding/([\w.-]+\.(?:svg|png|ico))", doc))
    for name in sorted(documented - on_disk):
        errors.append(f"docs/BRAND.md points at branding/{name}, which does not exist")
    for name in sorted(on_disk - documented):
        errors.append(f"branding/{name} exists but docs/BRAND.md never says what it is for")
    for name in sorted(on_disk):
        for forbidden in find_forbidden(name):
            errors.append(f"branding/{name}: asset name contains the brand \"{forbidden}\"")
    # The mark must scale: a fixed width/height defeats every small-size use.
    for name in sorted(on_disk):
        if not name.endswith(".svg"):
            continue
        svg = (BRANDING / name).read_text(encoding="utf-8")
        if "viewBox" not in svg:
            errors.append(f"branding/{name}: no viewBox, so it cannot scale")
        if re.search(r"<svg[^>]*\swidth=", svg):
            errors.append(f"branding/{name}: fixed width on <svg> overrides the size it is used at")
        if "<title>" not in svg:
            errors.append(f"branding/{name}: no <title>, so screen readers announce nothing")
    return len(on_disk)


def check_palette(errors: list[str]) -> None:
    tokens = json.loads(TOKENS.read_text(encoding="utf-8"))
    doc = DOC.read_text(encoding="utf-8")
    for theme in ("light", "dark"):
        accent = tokens["color"][theme]["accent"]
        if accent.lower() not in doc.lower():
            errors.append(
                f"docs/BRAND.md does not name the {theme} accent {accent} from design-tokens.json"
            )
    for hex_value in set(re.findall(r"#[0-9A-Fa-f]{6}", doc)):
        known = {
            value.lower()
            for theme in tokens["color"].values()
            for value in theme.values()
        }
        if hex_value.lower() not in known:
            errors.append(
                f"docs/BRAND.md names {hex_value}, which is not in design-tokens.json — "
                f"the tokens are the only place colours are defined"
            )


def main() -> int:
    if "--selftest" in sys.argv:
        assert find_forbidden("Import from Google Chrome") == ["google", "chrome"]
        assert find_forbidden("Built on Chromium") == [], "Chromium is the engine, not a brand claim"
        assert find_forbidden("Routed through Tor") == [], "the network may be named"
        assert find_forbidden("This is Tor Browser") == ["tor browser"], "claiming to be it is not"
        assert "hello" in visible_text("<style>p{}</style><!-- x --><p>hello</p>")
        assert "x" not in visible_text("<!-- x --><p>hello</p>")
        print("selftest OK")
        return 0

    errors: list[str] = []
    strings = check_strings(errors)
    mockups = check_mockups(errors)
    assets = check_assets(errors)
    check_palette(errors)

    if errors:
        print("branding check FAILED:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(
        f"branding OK: {assets} assets documented, {strings} strings and {mockups} mockups "
        f"free of other vendors' brands, palette matches design-tokens.json"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
