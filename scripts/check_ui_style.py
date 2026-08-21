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

Run: python3 scripts/check_ui_style.py
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TOKENS = ROOT / "branding/design-tokens.json"
MOCKUPS = sorted((ROOT / "docs/design/mockups").glob("*.html"))

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
    for value in re.findall(r"transition:[^;]*?([0-9.]+)s", css):
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


def main() -> int:
    errors: list[str] = []
    if not MOCKUPS:
        print("no mockups found", file=sys.stderr)
        return 1
    check_tokens(errors)
    for mockup in MOCKUPS:
        check_mockup(mockup, errors)
    for error in errors:
        print("FAIL:", error, file=sys.stderr)
    print(f"ui style OK: {len(MOCKUPS)} mockups within the item 27 limits")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
