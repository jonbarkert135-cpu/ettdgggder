#!/usr/bin/env python3
# Copyright 2026 The Bedrock Authors
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
"""Gate: no hidden cloud, and every remote feature stays optional (items 94, 95).

    python3 scripts/check_remote_features.py
    python3 scripts/check_remote_features.py --write     # regenerate the doc
    python3 scripts/check_remote_features.py --selftest

The table in `src_overrides/bedrock/privacy/network/remote_features.cc` lists
every way this browser may contact a server that is not the page you asked for.
The host test checks the table against items 94 and 95. This gate checks the
table against *the code*, which is the direction that catches the thing item 94
is actually about: a fetch that nobody declared.

  1. **No undeclared networking.** A module that gains request machinery —
     `SimpleURLLoader`, `URLLoaderFactory`, `network::ResourceRequest`,
     `fetch(`, `XMLHttpRequest`, a socket — must have a row in the table, and
     the row must say `kImplemented`. Adding a "quick config fetch" now fails
     the build instead of shipping.
  2. **No Bedrock endpoint.** Nothing in the tree may point at a host under our
     own name. We run no config service, no rules service, no list mirror, no
     sync server and no assistant; the only way to keep that true is to refuse
     the hostname everywhere, including in comments and docs examples.
  3. **The doc matches the table.** `docs/privacy/REMOTE.md` is generated here,
     so the published list of remote interactions cannot be shorter than the
     one in the source.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TABLE = ROOT / "src_overrides" / "bedrock" / "privacy" / "network" / "remote_features.cc"
SOURCES = ROOT / "src_overrides" / "bedrock"
DOC = ROOT / "docs" / "privacy" / "REMOTE.md"

# Machinery that means a request can actually leave the machine.
NETWORK_SYMBOLS = [
    "SimpleURLLoader", "URLLoaderFactory", "network::ResourceRequest",
    "URLFetcher", "mojo::Remote<network", "net::URLRequest",
    "XMLHttpRequest", "navigator.sendBeacon", "WebSocket(",
    "socket(", "getaddrinfo", "curl_easy",
]
JS_FETCH = re.compile(r"\bfetch\s*\(")

# Hosts that would mean we operate a service. Any spelling of our own name in a
# hostname position is wrong: the project has no servers.
BEDROCK_HOSTS = re.compile(
    r"https?://[\w.-]*\bbedrock(browser)?\.(com|org|net|io|dev|app|cloud|xyz)\b",
    re.I)

OPERATORS = {"kSiteYouVisit": "the site or engine you chose",
             "kThirdPartyYouChose": "a third party you picked",
             "kBedrockOperated": "OURS — never legal"}


def split_top_level(chunk: str) -> list[str]:
    """Split a C++ initialiser on commas outside braces and strings."""
    parts, depth, in_string, escaped, current = [], 0, False, False, ""
    for char in chunk:
        if in_string:
            current += char
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
            current += char
        elif char == "{":
            depth += 1
            current += char
        elif char == "}":
            depth -= 1
            current += char
        elif char == "," and depth == 0:
            parts.append(current)
            current = ""
        else:
            current += char
    if current.strip():
        parts.append(current)
    return parts


def literal(text: str) -> str:
    return "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', text)).replace('\\"', '"')


def parse_table(source: str) -> list[dict]:
    body = re.search(r"kFeatures = \{(.*?)\n  \};", source, re.S)
    if not body:
        sys.exit("remote_features.cc: could not find the kFeatures table")
    rows = []
    for chunk in re.findall(r"\{(.*?)\},\n", body.group(1) + "\n", re.S):
        fields = split_top_level(chunk)
        if len(fields) != 10:
            continue
        rows.append({
            "id": literal(fields[0]),
            "module": literal(fields[1]),
            "contacts": literal(fields[2]),
            "operator": fields[3].split("::")[-1].strip(),
            "status": fields[4].split("::")[-1].strip(),
            "on_by_default": "true" in fields[5],
            "inherent": "true" in fields[6],
            "how_to_disable": literal(fields[7]),
            "replacement": literal(fields[8]),
            "doc": literal(fields[9]),
        })
    if not rows:
        sys.exit("remote_features.cc: the table parsed as empty")
    return rows


def module_of(path: pathlib.Path) -> str:
    relative = path.relative_to(SOURCES).parts
    return "/".join(relative[:-1])


def check_undeclared_networking(rows: list[dict]) -> list[str]:
    declared = {row["module"]: row for row in rows}
    errors = []
    for path in sorted(SOURCES.rglob("*")):
        if path.suffix not in (".cc", ".h", ".js") or path.name.endswith("_test.cc"):
            continue
        if "fuzz" in path.parts:
            continue
        text = path.read_text(encoding="utf-8")
        code = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
        code = re.sub(r"(?m)^\s*//.*$", " ", code)
        # Prose about future work lives in string literals ("the URLLoaderFactory
        # hook is phase 7"): a name inside a string is not a call.
        code = re.sub(r'"(?:[^"\\]|\\.)*"', '""', code)
        hits = [symbol for symbol in NETWORK_SYMBOLS if symbol in code]
        if path.suffix == ".js" and JS_FETCH.search(code):
            hits.append("fetch(")
        if not hits:
            continue
        module = module_of(path)
        row = declared.get(module)
        if row is None:
            errors.append(
                f"{path.relative_to(ROOT)}: uses {hits[0]} but module {module!r} has no row "
                "in remote_features.cc — every remote interaction is declared (item 94)")
        elif row["status"] != "kImplemented":
            errors.append(
                f"{path.relative_to(ROOT)}: uses {hits[0]} while {row['id']!r} is still "
                "kPolicyOnly — say so in the table (item 90)")
    return errors


def check_no_bedrock_service() -> list[str]:
    errors = []
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or ".git" in path.parts or "sbom" in path.name:
            continue
        if path.name == "check_remote_features.py":  # its own self-test names one
            continue
        if path.suffix not in (".cc", ".h", ".js", ".json", ".md", ".gn", ".py", ".sh"):
            continue
        for match in BEDROCK_HOSTS.finditer(path.read_text(encoding="utf-8", errors="replace")):
            errors.append(f"{path.relative_to(ROOT)}: points at {match.group(0)} — "
                          "Bedrock operates no service (item 94)")
    return errors


def check_rows(rows: list[dict]) -> list[str]:
    errors = []
    for row in rows:
        if row["operator"] not in OPERATORS:
            errors.append(f"{row['id']}: unknown operator {row['operator']!r}")
        if row["operator"] == "kBedrockOperated":
            errors.append(f"{row['id']}: operated by us, which item 94 forbids outright")
        if row["on_by_default"] and not row["inherent"]:
            errors.append(f"{row['id']}: on by default (item 95: disabled by default)")
        if not row["how_to_disable"]:
            errors.append(f"{row['id']}: no way to turn it off (item 95)")
        if not (ROOT / row["doc"]).is_file():
            errors.append(f"{row['id']}: documented at {row['doc']}, which does not exist")
        if not (SOURCES / row["module"]).is_dir():
            errors.append(f"{row['id']}: module {row['module']!r} is not a source directory")
    return errors


def render(rows: list[dict]) -> str:
    lines = [
        "# Everything that leaves your machine",
        "",
        "**Roadmap items 94 and 95.** Generated from",
        "[`src_overrides/bedrock/privacy/network/remote_features.cc`]"
        "(../../src_overrides/bedrock/privacy/network/remote_features.cc)",
        "by `scripts/check_remote_features.py --write`; the gate fails if this file drifts",
        "from the table, and fails if any module gains networking code without a row.",
        "",
        "Two claims this page exists to make checkable:",
        "",
        "1. **Bedrock operates no servers.** No configuration service, no personalisation, no",
        "   cloud profile, no fingerprint database, no rules service, no assistant, no sync.",
        "   Every row below contacts either the site you asked for or a third party you chose.",
        "2. **The browser is complete with all of them off.** Nothing below is required for",
        "   Bedrock to start, browse, block, isolate or protect you.",
        "",
        "`policy` in the status column means the interaction is *permitted by the design and",
        "not built*: this overlay contains no network stack code today. Saying otherwise would",
        "be the kind of claim item 90 bans.",
        "",
        "<!-- BEGIN REMOTE FEATURES -->",
        "| Feature | Contacts | Operator | Default | Status | How to turn it off | Replaceable with |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for row in rows:
        default = "on (your own request)" if row["on_by_default"] else "off"
        status = "implemented" if row["status"] == "kImplemented" else "policy"
        lines.append(
            f"| `{row['id']}` | {row['contacts']} | {OPERATORS[row['operator']]} | {default} "
            f"| {status} | {row['how_to_disable']} | {row['replacement']} |")
    lines += ["<!-- END REMOTE FEATURES -->", ""]
    lines += [
        "## What is not here, and will not be",
        "",
        "Item 94 names the things a privacy browser is most often caught doing quietly. None",
        "of them exists in this tree, and the gate keeps it that way: cloud configuration,",
        "cloud personalisation, a cloud profile, a remote fingerprint database, a remote rules",
        "service, a server-side assistant. If any of them is ever built it is opt-in, modular,",
        "off by default, replaceable and documented here (item 95) — or it is not built.",
        "",
        "Telemetry is a separate promise with a separate gate: `scripts/check_no_telemetry.py`.",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        _selftest()
        return 0

    rows = parse_table(TABLE.read_text(encoding="utf-8"))
    expected = render(rows)
    if args.write:
        DOC.write_text(expected, encoding="utf-8")
        print(f"wrote {DOC.relative_to(ROOT)}: {len(rows)} remote features")
        return 0

    errors = check_rows(rows) + check_undeclared_networking(rows) + check_no_bedrock_service()
    if DOC.read_text(encoding="utf-8") != expected:
        errors.append("docs/privacy/REMOTE.md is out of date "
                      "(run scripts/check_remote_features.py --write)")
    if errors:
        print("remote feature check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    on = [row["id"] for row in rows if row["on_by_default"]]
    print(f"remote features OK: {len(rows)} declared, {len(on)} on by default "
          f"({', '.join(on) or 'none'}), no Bedrock-operated service")
    return 0


def _selftest() -> None:
    rows = parse_table(TABLE.read_text(encoding="utf-8"))
    assert len(rows) >= 6, rows
    assert {row["id"] for row in rows} >= {"doh_resolver", "search_suggestions"}
    assert check_rows(rows) == [], check_rows(rows)

    ours = dict(rows[0], id="cloud_config", operator="kBedrockOperated")
    assert any("item 94" in e for e in check_rows([ours])), "a Bedrock service must fail"
    always = dict(rows[0], id="rules_service", on_by_default=True, inherent=False)
    assert any("disabled by default" in e for e in check_rows([always]))
    no_doc = dict(rows[0], id="x", doc="docs/does-not-exist.md")
    assert any("does not exist" in e for e in check_rows([no_doc]))
    assert BEDROCK_HOSTS.search("https://config.bedrock.io/v1"), "our own host must match"
    assert not BEDROCK_HOSTS.search("https://example.com/bedrock"), "a path is not a host"
    print("selftest OK")


if __name__ == "__main__":
    sys.exit(main())
