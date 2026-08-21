#!/usr/bin/env python3
"""Platform abstraction gate (roadmap items 62, 63, 64).

Two rules, both easy to break by accident and expensive to unwind later:

  * **Platform macros live in the platform layer.** `#ifdef _WIN32` in the
    privacy engine turns one codebase into three that share a directory, and the
    third one rots unnoticed. Only files under src_overrides/bedrock/platform/
    may name a platform macro.
  * **No desktop environment is assumed.** A hardcoded GNOME or KDE path is why
    a browser feels broken to everyone on a different desktop. Naming one in a
    *failure mode* — a warning about that mistake — is allowed; naming one in a
    requirement is not.

It also checks docs/PLATFORMS.md against the code table, so the documented
integration points and package formats cannot drift from the ones in the source.

Usage: python3 scripts/check_platform.py [--selftest]
"""

from __future__ import annotations

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
SOURCES = REPO / "src_overrides"
PLATFORM_DIR = SOURCES / "bedrock" / "platform"
CODE = PLATFORM_DIR / "platform_support.cc"
DOC = REPO / "docs" / "PLATFORMS.md"

PLATFORM_MACROS = re.compile(
    r"\b(?:defined\s*\(\s*)?(_WIN32|_WIN64|__linux__|__APPLE__|__unix__|"
    r"OS_WIN|OS_LINUX|OS_MAC|BUILDFLAG\(IS_WIN\)|BUILDFLAG\(IS_LINUX\)|"
    r"BUILDFLAG\(IS_MAC\))"
)
DESKTOPS = ("gnome", "kde", "xfce", "cinnamon", "mate", "lxqt", "plasma",
            "unity", "budgie")
REQUIREMENT = re.compile(
    r"\{Platform::k(?P<platform>\w+), IntegrationPoint::k(?P<point>\w+), "
    r"Owner::k(?P<owner>\w+),\s*(?P<body>.*?)\},\n",
    re.S,
)
PACKAGE = re.compile(r'\{"(?P<name>[\w.]+)", (?P<produced>true|false),')


def check_macros(errors: list[str]) -> int:
    """Platform macros outside the platform layer."""
    checked = 0
    for path in sorted(SOURCES.rglob("*.[hc]*")):
        if PLATFORM_DIR in path.parents:
            continue
        checked += 1
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if line.lstrip().startswith("//"):
                continue  # a comment may discuss the rule it is documenting
            match = PLATFORM_MACROS.search(line)
            if match:
                relative = path.relative_to(REPO)
                errors.append(
                    f"{relative}:{number}: {match.group(1)} outside "
                    f"src_overrides/bedrock/platform/ — ask the platform layer for a "
                    f"capability instead"
                )
    return checked


def check_requirements(text: str, errors: list[str]) -> int:
    """Every requirement string is filled in and assumes no desktop environment."""
    requirements = list(REQUIREMENT.finditer(text))
    for match in requirements:
        body = match.group("body")
        strings = re.findall(r'"((?:[^"\\]|\\.)*)"', body)
        joined = " ".join(strings)
        # The first literal is the requirement, the rest is the failure mode.
        requirement_text = strings[0].lower() if strings else ""
        if len(joined) < 60:
            errors.append(
                f"{match.group('platform')}/{match.group('point')}: requirement is too "
                f"thin to hold anyone to"
            )
        for desktop in DESKTOPS:
            if desktop in requirement_text:
                errors.append(
                    f"{match.group('platform')}/{match.group('point')}: requirement "
                    f"assumes {desktop} — detect capabilities, do not name desktops"
                )
    if len(requirements) < 30:
        errors.append(
            f"only parsed {len(requirements)} platform requirements — the parser or the "
            f"table broke"
        )
    return len(requirements)


def check_docs(text: str, errors: list[str]) -> None:
    doc = DOC.read_text(encoding="utf-8").lower()
    for match in REQUIREMENT.finditer(text):
        point = match.group("point")
        # kSystemColorScheme -> "system color scheme" -> documented as a row
        words = re.findall(r"[A-Z][a-z]+|[A-Z]+(?![a-z])", point)
        if not any(word.lower() in doc for word in words):
            errors.append(f"{point}: integration point missing from docs/PLATFORMS.md")
    for match in PACKAGE.finditer(text):
        if match.group("name").lower() not in doc:
            errors.append(f"{match.group('name')}: package format missing from docs/PLATFORMS.md")


def main() -> int:
    if "--selftest" in sys.argv:
        probe: list[str] = []
        check_requirements(
            '{Platform::kLinux, IntegrationPoint::kSystemTheme, Owner::kBedrock,\n'
            '     "Read the theme from the GNOME settings daemon and apply it to every '
            'surface of the browser",\n     "some failure"},\n',
            probe,
        )
        assert any("assumes gnome" in error for error in probe), probe
        probe = []
        assert PLATFORM_MACROS.search("#if defined(OS_WIN)"), "macro regex broke"
        assert PLATFORM_MACROS.search("#ifdef __linux__"), "macro regex broke"
        assert not PLATFORM_MACROS.search("int windows = 3;"), "macro regex too greedy"
        print("selftest OK")
        return 0

    errors: list[str] = []
    text = CODE.read_text(encoding="utf-8")
    files = check_macros(errors)
    requirements = check_requirements(text, errors)
    check_docs(text, errors)

    if errors:
        print("platform check FAILED:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(
        f"platform OK: {requirements} integration requirements documented, "
        f"{files} files free of platform macros, no desktop environment assumed"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
