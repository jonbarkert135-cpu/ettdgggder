#!/usr/bin/env python3
"""Generate build/sbom.json (CycloneDX 1.5) from the third-party inventory, and
verify it in CI.

  python3 scripts/generate_sbom.py            # write build/sbom.json
  python3 scripts/generate_sbom.py --check    # fail if it is out of date

Roadmap item 42. The SBOM is generated from `docs/THIRD_PARTY.md` and
`build/dependency-hashes.txt` rather than written by hand, so it cannot drift
from the inventory the provenance gate already enforces. A hand-maintained SBOM
is a document; a generated one is a fact about the build.
"""

from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
INVENTORY = REPO / "docs" / "THIRD_PARTY.md"
HASHES = REPO / "build" / "dependency-hashes.txt"
PIN = REPO / "build" / "chromium.pin"
SBOM = REPO / "build" / "sbom.json"


def read_inventory() -> list[dict[str, str]]:
    block = re.search(r"<!-- BEGIN INVENTORY -->(.*?)<!-- END INVENTORY -->",
                      INVENTORY.read_text(), re.S)
    if not block:
        sys.exit("THIRD_PARTY.md: inventory markers not found")
    rows = []
    for line in block.group(1).strip().splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) != 6 or cells[0] == "Project" or set(cells[0]) <= set("- "):
            continue
        rows.append(dict(zip(
            ["project", "repository", "version", "license", "mode", "notice"], cells)))
    return rows


def read_hashes() -> dict[str, str]:
    hashes = {}
    if HASHES.is_file():
        for line in HASHES.read_text().splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            digest, _, name = line.partition("  ")
            hashes[name.strip()] = digest.strip()
    return hashes


def build_sbom() -> dict:
    rows = read_inventory()
    hashes = read_hashes()
    pin = dict(
        line.split("=", 1) for line in PIN.read_text().splitlines()
        if line.strip() and not line.startswith("#"))

    components = []
    for row in rows:
        component = {
            "type": "library",
            "name": row["project"],
            "version": row["version"],
            "licenses": [{"license": {"name": row["license"]}}],
            "externalReferences": [
                {"type": "vcs", "url": row["repository"]},
            ],
            "properties": [
                {"name": "bedrock:reuse_mode", "value": row["mode"]},
                {"name": "bedrock:notice", "value": f"THIRD_PARTY_NOTICES/{row['notice']}"},
            ],
        }
        digest = hashes.get(row["project"])
        if digest:
            component["hashes"] = [{"alg": "SHA-256", "content": digest}]
        components.append(component)

    return {
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "version": 1,
        "metadata": {
            "component": {
                "type": "application",
                "name": "Bedrock Browser",
                "version": pin.get("version", "unknown").strip(),
            },
            "properties": [
                {"name": "bedrock:chromium_commit", "value": pin.get("commit", "").strip()},
                {"name": "bedrock:generated_from",
                 "value": "docs/THIRD_PARTY.md + build/dependency-hashes.txt"},
                {"name": "bedrock:generator", "value": "scripts/generate_sbom.py"},
            ],
        },
        "components": components,
    }


def main(argv: list[str]) -> int:
    sbom = build_sbom()
    text = json.dumps(sbom, indent=2, sort_keys=True) + "\n"
    if "--check" in argv:
        if not SBOM.is_file():
            print("SBOM check FAILED: build/sbom.json is missing "
                  "(run scripts/generate_sbom.py)")
            return 1
        current = SBOM.read_text()
        if current != text:
            print("SBOM check FAILED: build/sbom.json is out of date with "
                  "docs/THIRD_PARTY.md (run scripts/generate_sbom.py)")
            return 1
        digest = hashlib.sha256(current.encode()).hexdigest()[:16]
        print(f"SBOM OK: {len(sbom['components'])} components, digest {digest}")
        return 0
    SBOM.write_text(text)
    print(f"wrote {SBOM.relative_to(REPO)}: {len(sbom['components'])} components")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
