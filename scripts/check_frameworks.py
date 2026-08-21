#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Gate: no JS framework where Chromium's own infrastructure is the answer.

    python3 scripts/check_frameworks.py
    python3 scripts/check_frameworks.py --selftest

Roadmap item 78. Chromium already ships everything a browser UI needs -- views
for the native surfaces, and for WebUI a document, custom elements, shadow DOM
and fetch. Adding React on top buys a component model the browser already has,
and pays for it in a bundle that must be shipped, updated, audited, and kept out
of the way of the renderer it is drawing over. A browser whose settings page is
a single-page app is a "website packaged as an application"; that is the thing
item 78 forbids.

Four rules, all mechanical:

  1. No framework or bundler appears as a dependency anywhere in the tree.
  2. No Node package manifest or lockfile exists -- there is no npm install step
     between a checkout and a build.
  3. No HTML, JS or CSS file loads anything off this machine (a CDN <script> is
     a framework dependency plus a network dependency plus a fingerprinting
     surface, in one line).
  4. The rule is written down where a contributor will meet it, in
     docs/adr/0006-no-ui-frameworks.md.

Naming a framework in prose is fine -- the ADR has to explain what it rules out.
The scan is over code and manifests.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ADR = ROOT / "docs" / "adr" / "0006-no-ui-frameworks.md"

CODE_SUFFIXES = {".js", ".ts", ".tsx", ".jsx", ".html", ".css", ".gn", ".gni"}
MANIFESTS = {
    "package.json", "package-lock.json", "yarn.lock", "pnpm-lock.yaml",
    "bower.json", "webpack.config.js", "vite.config.js", "rollup.config.js",
    ".babelrc", "tsconfig.json",
}

# Frameworks, UI kits and bundlers. The point is not that these are bad
# software; it is that none of them is the answer to a question a browser has.
FRAMEWORKS = [
    "react", "react-dom", "preact", "vue", "angular", "@angular", "svelte",
    "ember", "backbone", "knockout", "alpinejs", "jquery", "lit-element",
    "polymer", "bootstrap", "tailwindcss", "material-ui", "@mui", "chakra-ui",
    "bulma", "foundation-sites", "webpack", "rollup", "parcel", "esbuild",
    "vite", "babel", "gulp", "grunt", "browserify",
    # The two application shells are deliberately not named here:
    # check_languages.py (ADR 0004) already fails the build on the mere word,
    # in every file except its own.
]
# How a dependency actually enters a file, as opposed to being discussed in one.
IMPORT = re.compile(
    r"""(?:^|\n)\s*(?:import\s[^\n]*from\s*['"]|import\s*['"]|require\(\s*['"])"""
    r"""(?P<spec>[^'"]+)['"]""")
SCRIPT_SRC = re.compile(r"""(?:src|href)\s*=\s*['"](?P<url>[^'"]+)['"]""",
                        re.IGNORECASE)
OFF_MACHINE = re.compile(
    r"""^(?:https?:)?//(?!127\.0\.0\.1|localhost)""", re.IGNORECASE)


def code_files() -> list[pathlib.Path]:
    files = []
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or ".git" in path.parts:
            continue
        if path.suffix in CODE_SUFFIXES:
            files.append(path)
    return files


def check_manifests(errors: list[str]) -> None:
    for path in sorted(ROOT.rglob("*")):
        if ".git" in path.parts or not path.is_file():
            continue
        if path.name in MANIFESTS:
            errors.append(
                f"{path.relative_to(ROOT)}: a Node manifest means a build step "
                "and a dependency tree Bedrock does not have (item 78)")


def scan_text(rel: str, text: str, errors: list[str]) -> None:
    for match in IMPORT.finditer(text):
        spec = match.group("spec")
        bare = spec.split("/")[0] if not spec.startswith(".") else ""
        for framework in FRAMEWORKS:
            if spec == framework or bare == framework or spec.startswith(framework + "/"):
                errors.append(f"{rel}: imports {spec} — item 78 (use the platform)")
    for match in SCRIPT_SRC.finditer(text):
        url = match.group("url")
        if OFF_MACHINE.match(url) or url.startswith(("http://", "https://")):
            if not re.match(r"https?://(127\.0\.0\.1|localhost)", url):
                errors.append(f"{rel}: loads {url} from off this machine")
        for framework in FRAMEWORKS:
            if framework in url.lower():
                errors.append(f"{rel}: pulls in {framework} via {url}")


def check_sources(errors: list[str]) -> None:
    for path in code_files():
        scan_text(path.relative_to(ROOT).as_posix(),
                  path.read_text(encoding="utf-8", errors="replace"), errors)


def check_adr(errors: list[str]) -> None:
    if not ADR.is_file():
        errors.append("docs/adr/0006-no-ui-frameworks.md: missing")
        return
    text = ADR.read_text(encoding="utf-8")
    for phrase in ("website packaged as an application", "custom elements"):
        if phrase not in text:
            errors.append(f"docs/adr/0006-no-ui-frameworks.md: does not say '{phrase}'")


def selftest() -> int:
    errors: list[str] = []
    scan_text("fake.ts", 'import React from "react";\n', errors)
    assert errors, "a react import must be caught"
    errors = []
    scan_text("fake.html",
              '<script src="https://cdn.example.com/vue.js"></script>', errors)
    assert len(errors) == 2, f"CDN and framework must both fire, got {errors}"
    errors = []
    scan_text("ok.html",
              '<script src="./local.js"></script>\n'
              '<link href="style.css" rel="stylesheet">', errors)
    assert not errors, f"local assets must pass, got {errors}"
    errors = []
    scan_text("ok.ts", 'import {Panel} from "./panel.js";\n', errors)
    assert not errors, f"a relative import must pass, got {errors}"
    print("check_frameworks selftest OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return selftest()

    errors: list[str] = []
    check_manifests(errors)
    check_sources(errors)
    check_adr(errors)
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1
    print(f"frameworks OK: {len(code_files())} web-language files, no framework, "
          "no bundler, no off-machine asset")
    return 0


if __name__ == "__main__":
    sys.exit(main())
