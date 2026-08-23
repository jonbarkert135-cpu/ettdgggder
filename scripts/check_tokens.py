#!/usr/bin/env python3
"""Design items 31 and 32, as a gate.

The rule: a shipped page names a *token*, never a colour. `background: #121212`
in one page is harmless; the same line in a hundred pages is a theme system that
does not work, because changing the theme means finding all hundred. So this
gate reads every page under src_overrides/bedrock/ui and fails on a raw colour
outside the generated stylesheet.

It also checks that the semantic vocabulary the interface is written against
actually exists in tokens.css, so a page cannot reference a token nobody
generates and silently fall back to a browser default.

Run: python3 scripts/check_tokens.py
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
UI = ROOT / "src_overrides/bedrock/ui"
TOKENS_CSS = UI / "tokens.css"

# Written in CSS as --background-primary; item 31 lists them with dots.
REQUIRED = [
    "background-primary",
    "background-secondary",
    "surface-default",
    "surface-elevated",
    "surface-glass",
    "text-primary",
    "text-secondary",
    "text-muted",
    "border-subtle",
    "border-default",
    "border-focus",
    "accent-primary",
    "shadow-soft",
    "shadow-elevated",
    "blur-subtle",
    "blur-strong",
    "radius-sm",
    "radius-md",
    "radius-lg",
]

# A hex colour, or an rgb()/rgba()/hsl() literal, in a page's own CSS.
COLOR = re.compile(r"#[0-9A-Fa-f]{3,8}\b|\brgba?\(|\bhsla?\(")
# HTML entities (&#9906;) are not colours.
ENTITY = re.compile(r"&#\d+;")


def main() -> int:
    errors: list[str] = []

    css = TOKENS_CSS.read_text() if TOKENS_CSS.exists() else ""
    if not css:
        print("check_tokens FAILED: tokens.css is missing", file=sys.stderr)
        return 1
    for name in REQUIRED:
        if f"--{name}:" not in css:
            errors.append(f"tokens.css does not define --{name}")

    pages = sorted(p for p in UI.rglob("*") if p.suffix in (".html", ".js"))
    for page in pages:
        text = ENTITY.sub("", page.read_text())
        for line_number, line in enumerate(text.splitlines(), 1):
            if COLOR.search(line):
                errors.append(
                    f"{page.name}:{line_number}: raw colour — use a token: {line.strip()}"
                )

    if errors:
        print("token check FAILED:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print(
        f"tokens OK: {len(REQUIRED)} semantic tokens defined, "
        f"{len(pages)} pages free of raw colours"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
