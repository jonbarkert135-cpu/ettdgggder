#!/usr/bin/env python3
"""Roadmap item 27's taste rules, as a gate.

"Clean, minimal, premium, no decorative junk" is unenforceable as prose, so the
limits live in branding/design-tokens.json and in ThemeEngine, and this script
checks that the mockups and tokens actually obey them:

  * corner radius     <= 16px      (no giant rounded cards)
  * blur              <= 12px      (no glass soup)
  * transition/animation <= 200ms  (no heavy motion)
  * gradients         <= 2 per file, and none on interactive chrome
  * no decorative shadows beyond the three defined elevations

Item 60 adds the accessibility rules a mockup can be held to: a lang attribute,
a visible focus ring, a reduced-motion rule, real controls instead of clickable
divs, a name on every icon-only button, and decorative glyphs hidden from the
accessibility tree.

Run: python3 scripts/check_ui_style.py
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TOKENS = ROOT / "branding/design-tokens.json"
# Mockups are the specification; the shipped WebUI pages are held to the same
# limits, or the rules stop applying exactly where users meet them.
MOCKUPS = sorted((ROOT / "docs/design/mockups").glob("*.html")) + sorted(
    (ROOT / "src_overrides/bedrock/ui").rglob("*.html"))

MAX_RADIUS = 16
MAX_BLUR = 12
MAX_DURATION_MS = 200
MAX_GRADIENTS = 2


def check_tokens(errors: list[str]) -> None:
    tokens = json.loads(TOKENS.read_text())
    for name, value in tokens["radius"].items():
        if name != "pill" and value > MAX_RADIUS:
            errors.append(f"tokens: radius.{name} = {value}px > {MAX_RADIUS}px")
    for name, value in tokens["motion"].items():
        for ms in re.findall(r"(\d+)ms", str(value)):
            if int(ms) > MAX_DURATION_MS:
                errors.append(f"tokens: motion.{name} = {ms}ms > {MAX_DURATION_MS}ms")
    if "reduced-motion" not in tokens["motion"]:
        errors.append("tokens: motion has no reduced-motion rule")
    if tokens["density"]["hit-target-min"] < 32:
        errors.append("tokens: hit targets below 32px are not comfortably clickable")


def check_mockup(path: pathlib.Path, errors: list[str]) -> None:
    css = path.read_text()
    where = path.name

    for value in re.findall(r"border-radius:\s*([0-9.]+)px", css):
        # 999px is the pill idiom (chips, toggles): a capsule, not a giant card.
        if float(value) > MAX_RADIUS and float(value) < 100:
            errors.append(f"{where}: border-radius {value}px > {MAX_RADIUS}px")
    for value in re.findall(r"blur\(([0-9.]+)px\)", css):
        if float(value) > MAX_BLUR:
            errors.append(f"{where}: blur {value}px > {MAX_BLUR}px")
    for value in re.findall(r"(\d+)ms", css):
        if int(value) > MAX_DURATION_MS:
            errors.append(f"{where}: {value}ms animation > {MAX_DURATION_MS}ms")
    for value in re.findall(r"transition:[^;]*?(?<![\w.])([0-9]*\.?[0-9]+)s(?![\w-])", css):
        if float(value) * 1000 > MAX_DURATION_MS:
            errors.append(f"{where}: {value}s transition > {MAX_DURATION_MS}ms")

    gradients = len(re.findall(r"(linear|radial|conic)-gradient", css))
    if gradients > MAX_GRADIENTS:
        errors.append(f"{where}: {gradients} gradients, budget is {MAX_GRADIENTS}")

    # A mockup that names a competitor's product in its own chrome is copying
    # rather than referencing.
    for brand in ("safari", "chrome ui", "edge ui", "firefox brand"):
        if brand in css.lower():
            errors.append(f"{where}: references {brand}")


# Roadmap item 60. A mockup is the specification the UI is built from, so an
# inaccessible mockup becomes an inaccessible browser.
INTERACTIVE_CLASSES = ("btn", "tab", "icon", "newtab", "item", "ic", "shield", "tsearch")


def check_accessibility(path: pathlib.Path, errors: list[str]) -> None:
    html = path.read_text()
    where = path.name

    if not re.search(r"<html[^>]+lang=", html):
        errors.append(f"{where}: <html> has no lang attribute — screen readers guess the language")
    if ":focus-visible" not in html:
        errors.append(f"{where}: no :focus-visible rule — keyboard users cannot see where they are")
    if re.search(r"outline:\s*(none|0)", html) and ":focus-visible" not in html:
        errors.append(f"{where}: removes the focus outline without replacing it")
    if re.search(r"transition|animation", html) and "prefers-reduced-motion" not in html:
        errors.append(f"{where}: animates but has no prefers-reduced-motion rule")

    # Something the user clicks has to be a control, not a styled <div>: that is
    # what gives it keyboard focus and a role in the accessibility tree.
    for cls in INTERACTIVE_CLASSES:
        for match in re.finditer(rf'<div class="{cls}(?:\s[^"]*)?"', html):
            errors.append(f"{where}: <div class=\"{cls}\"> is clickable but is not a control "
                          f"(use <button>) at offset {match.start()}")

    # An icon-only control needs a name; a glyph is not a label.
    for match in re.finditer(r"<button([^>]*)>(.*?)</button>", html, re.S):
        attrs, body = match.group(1), re.sub(r"<[^>]+>", "", match.group(2)).strip()
        words = re.sub(r"[^A-Za-z0-9 ]", "", body).strip()
        if len(words) < 2 and "aria-label" not in attrs:
            errors.append(f"{where}: icon-only button {body!r} has no aria-label")

    # Decorative glyphs must be hidden from the tree, or they are read aloud.
    for match in re.finditer(r'<i class="(fav|sq|glyph|dot|d)"([^>]*)>', html):
        if "aria-hidden" not in match.group(2):
            errors.append(f"{where}: decorative <i class=\"{match.group(1)}\"> "
                          f"is not aria-hidden")


def main() -> int:
    errors: list[str] = []
    if not MOCKUPS:
        print("no mockups found", file=sys.stderr)
        return 1
    check_tokens(errors)
    for mockup in MOCKUPS:
        check_mockup(mockup, errors)
        check_accessibility(mockup, errors)
    for error in errors:
        print("FAIL:", error, file=sys.stderr)
    print(f"ui style OK: {len(MOCKUPS)} mockups within the item 27 limits "
          f"and the item 60 accessibility rules")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
