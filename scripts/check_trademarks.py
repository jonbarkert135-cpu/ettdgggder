#!/usr/bin/env python3
"""Other people's trademarks and other people's interfaces (items 92, 96, 97).

`check_branding.py` guards *our* identity: the name, the mark, the palette.
This one guards the other side of the same line — the point where another
vendor's brand or interface starts standing in for ours.

Naming another browser is legitimate and unavoidable: the import step must say
Chrome, the search step must say Google, the DoH preset list must name its
operators, and the Tor disclaimer must say "Bedrock is not the Tor Browser".
Trademark law calls that nominative use and it is exactly what item 92 leaves
alone. What item 92 forbids is a vendor's name or mark standing where *Bedrock's*
identity belongs, or any wording that suggests the other vendor endorsed,
approved or shipped this browser. Those two are what this script looks for:

  1. **Affiliation.** A vendor name near a word like "official", "powered by",
     "certified" or "in partnership with", in anything a user can read.
  2. **Identity.** A vendor name inside a string that names the product itself:
     app name, product name, window title, brand, logo or icon.
  3. **Marks on disk.** An image or icon file anywhere in the tree named after
     another vendor — the quiet way a foreign logo arrives and stays.
  4. **A borrowed interface vocabulary** (items 96, 97). CSS custom properties,
     classes and ids carrying another browser's prefix are the fingerprint of
     copied UI; our own tokens are in branding/design-tokens.json.
  5. **An undeclared influence** (item 96). Every research note under
     docs/research/ has a stance in docs/IDENTITY.md saying what Bedrock took
     from that project and what it did not. A new influence cannot be studied
     into the product without stating one.

Usage: python3 scripts/check_trademarks.py [--selftest]
"""

from __future__ import annotations

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
IDENTITY = REPO / "docs" / "IDENTITY.md"
RESEARCH = REPO / "docs" / "research"

# Browser and platform vendors whose brands can plausibly turn up in our UI.
VENDORS = [
    "chrome", "chromium", "google", "firefox", "mozilla", "brave", "safari",
    "apple", "edge", "microsoft", "opera", "vivaldi", "tor browser", "duckduckgo",
]

# Words that turn a mention into a claim of endorsement.
AFFILIATION = [
    "official", "officially", "powered by", "in partnership", "partnership with",
    "certified", "endorsed", "approved by", "authorised by", "authorized by",
    "a product of", "brought to you by", "on behalf of",
]

# Identifiers whose value *is* the product's identity.
IDENTITY_HINTS = [
    "app_name", "appname", "kappname", "product_name", "productname",
    "kproductname", "window_title", "windowtitle", "brand", "kbrand",
    "logo", "icon_name", "iconname",
]

# Another browser's UI vocabulary, as it appears in CSS and markup.
BORROWED_SELECTORS = [
    "--brave-", "--moz-", "--chrome-", "--edge-", "--safari-",
    "brave-", "moz-", "firefox-", "chromium-ui-",
]

STRING_LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')
UI = REPO / "src_overrides" / "bedrock" / "ui"
MOCKUPS = REPO / "docs" / "design" / "mockups"
IMAGE_SUFFIXES = {".png", ".svg", ".ico", ".jpg", ".jpeg", ".webp", ".gif"}
# How close a vendor name and an affiliation word have to be to read as one claim.
NEAR = 60


def vendors_in(text: str) -> list[tuple[str, int]]:
    lowered = text.lower()
    found = []
    for vendor in VENDORS:
        for match in re.finditer(rf"\b{re.escape(vendor)}\b", lowered):
            found.append((vendor, match.start()))
    return found


def affiliation_claim(text: str) -> str | None:
    """A vendor name and an endorsement word close enough to be one sentence."""
    lowered = text.lower()
    for vendor, where in vendors_in(text):
        for word in AFFILIATION:
            for match in re.finditer(re.escape(word), lowered):
                if abs(match.start() - where) <= NEAR:
                    return f'"{word}" next to "{vendor}"'
    return None


def visible_text(html: str) -> str:
    html = re.sub(r"<!--.*?-->", " ", html, flags=re.S)
    html = re.sub(r"<(style|script)\b.*?</\1>", " ", html, flags=re.S | re.I)
    return re.sub(r"<[^>]+>", " ", html)


def user_visible_strings() -> list[tuple[str, str]]:
    """(where, text) for everything a user can end up reading."""
    out: list[tuple[str, str]] = []
    for path in sorted((REPO / "src_overrides").rglob("*.cc")):
        if path.name.endswith("_test.cc"):
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            for text in STRING_LITERAL.findall(line):
                out.append((f"{path.relative_to(REPO)}:{number}", text))
    for folder in (UI, MOCKUPS):
        for path in sorted(folder.rglob("*.html")):
            out.append((str(path.relative_to(REPO)), visible_text(path.read_text(encoding="utf-8"))))
    return out


def check_affiliation() -> list[str]:
    errors = []
    for where, text in user_visible_strings():
        claim = affiliation_claim(text)
        if claim:
            errors.append(f"{where}: {claim} reads as a claim of affiliation (item 92): "
                          f"{text.strip()[:90]!r}")
    return errors


def check_identity_strings() -> list[str]:
    """A vendor name may describe another product; it may not name ours."""
    errors = []
    for path in sorted((REPO / "src_overrides").rglob("*.cc")):
        if path.name.endswith("_test.cc"):
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            before = line.split("=")[0].lower() if "=" in line else ""
            if not any(hint in before for hint in IDENTITY_HINTS):
                continue
            for text in STRING_LITERAL.findall(line):
                for vendor, _ in vendors_in(text):
                    errors.append(
                        f"{path.relative_to(REPO)}:{number}: the product's own identity is "
                        f'set to a string containing "{vendor}" (item 92)')
    return errors


def check_marks_on_disk() -> list[str]:
    errors = []
    for path in sorted(REPO.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in IMAGE_SUFFIXES:
            continue
        if ".git" in path.parts:
            continue
        stem = re.sub(r"[-_.]", " ", path.stem.lower())
        for vendor in VENDORS:
            if re.search(rf"\b{re.escape(vendor)}\b", stem):
                errors.append(f"{path.relative_to(REPO)}: an image named after {vendor} "
                              "— Bedrock ships no other vendor's mark (item 92)")
    return errors


def check_borrowed_ui() -> list[str]:
    """Items 96/97: our interface is ours, down to the names in the CSS."""
    seen: set[tuple[str, str]] = set()
    errors = []
    for folder in (UI, MOCKUPS):
        for path in sorted(list(folder.rglob("*.html")) + list(folder.rglob("*.js"))):
            text = path.read_text(encoding="utf-8")
            # Strip comments: prose about another browser's ideas is allowed and
            # expected, it is the *code* that must not borrow their vocabulary.
            text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
            text = re.sub(r"(?m)^\s*//.*$", " ", text)
            text = re.sub(r"<!--.*?-->", " ", text, flags=re.S)
            for token in BORROWED_SELECTORS:
                for match in re.finditer(rf"[-.#\w]*{re.escape(token)}[\w-]*", text):
                    name = match.group(0)
                    if not name.startswith(("--", ".", "#")) or (path.name, name) in seen:
                        continue
                    seen.add((path.name, name))
                    errors.append(
                        f"{path.relative_to(REPO)}: {name!r} borrows another browser's "
                        "interface vocabulary (items 96, 97)")
    return errors


def check_declared_influences() -> list[str]:
    """Item 96: one product, and every influence on it stated in the open."""
    if not IDENTITY.is_file():
        return ["docs/IDENTITY.md is missing: items 96 and 97 need a stated identity"]
    identity = IDENTITY.read_text(encoding="utf-8")
    errors = []
    for path in sorted(RESEARCH.glob("*.md")):
        if path.name == "README.md":
            continue
        if f"research/{path.name}" not in identity:
            errors.append(
                f"docs/research/{path.name} studies a project that docs/IDENTITY.md never "
                "takes a position on — say what Bedrock took from it and what it did not "
                "(item 96)")
    for required in ("does not copy", "one product"):
        if required not in identity.lower():
            errors.append(f"docs/IDENTITY.md: no statement containing {required!r} "
                          "(items 96, 97)")
    return errors


def main() -> int:
    errors = (check_affiliation() + check_identity_strings() + check_marks_on_disk()
              + check_borrowed_ui() + check_declared_influences())
    if errors:
        print("trademark check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    strings = len(user_visible_strings())
    influences = len([p for p in RESEARCH.glob("*.md") if p.name != "README.md"])
    print(f"trademarks OK: {strings} user-visible strings, {influences} declared influences")
    return 0


def _selftest() -> None:
    """Each rule, shown catching the thing it exists for."""
    assert affiliation_claim("The official Chrome experience") is not None
    assert affiliation_claim("Powered by Brave Shields") is not None
    # Nominative use, which item 92 permits and item 98 requires.
    assert affiliation_claim("Import bookmarks and history from Chrome") is None
    assert affiliation_claim("Bedrock is not the Tor Browser.") is None
    # Distance matters: two unrelated sentences are not a claim.
    far = "Chrome" + " word" * 40 + " official documentation"
    assert affiliation_claim(far) is None
    assert vendors_in("chromed steel")== [], "substring match would flag ordinary prose"
    assert check_declared_influences() == [], "IDENTITY.md should cover every research note"
    assert check_identity_strings() == [] and check_marks_on_disk() == []
    assert check_borrowed_ui() == [] and check_affiliation() == []
    print("selftest OK")


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        _selftest()
    else:
        sys.exit(main())
