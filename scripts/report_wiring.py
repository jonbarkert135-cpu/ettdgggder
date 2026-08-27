"""Report which Bedrock modules the built browser can actually reach.

Not a gate: it needs a local build, which CI does not have. It answers one
question honestly — how much of the overlay is wired into a running browser
versus compiled and never called — by listing the classes whose names appear as
symbols in the linked binaries. Run it after a build and paste the summary into
docs/WIRING.md with the build date.

Usage: python3 scripts/report_wiring.py [--out docs/WIRING.md]
       BEDROCK_OUT=/path/to/out/Release python3 scripts/report_wiring.py
"""
import json, pathlib, re, subprocess, collections

import os

REPO = pathlib.Path(__file__).resolve().parent.parent
SRC = REPO / "src_overrides" / "bedrock"
OUT_DIR = pathlib.Path(os.environ.get("BEDROCK_OUT", "/work/chromium/src/out/Release"))
BINARIES = [OUT_DIR / "chrome",
            OUT_DIR / "libservices_network_network_service.so"]

def linked_symbols() -> set[str]:
    names = set()
    missing = [b for b in BINARIES if not b.exists()]
    if missing:
        raise SystemExit(
            "no local build to inspect: " + ", ".join(str(m) for m in missing) +
            "\nBuild first (build/LOCAL_BUILD_HANDOFF.md) or set BEDROCK_OUT.")
    for binary in BINARIES:
        out = subprocess.run(["nm", "-C", str(binary)], capture_output=True, text=True).stdout
        for line in out.splitlines():
            if "bedrock::" in line:
                names.add(line.split("bedrock::", 1)[1])
    return names

def module_of(path: pathlib.Path) -> str:
    rel = path.relative_to(SRC)
    return str(rel.parent)

def main() -> None:
    symbols = linked_symbols()
    # Exact token match: a class counts as linked only when its own name is one
    # of the :: components of a linked symbol. Substring matching lied — the
    # class Profile "matched" bedrock::settings::FactoryProfileName.
    tokens = set()
    for name in symbols:
        for part in re.split(r"[^A-Za-z0-9_]+", name):
            tokens.add(part)
    stats = collections.defaultdict(lambda: {"files": 0, "loc": 0, "classes": set(), "linked": set()})
    for path in sorted(SRC.rglob("*.h")):
        if path.name.endswith("_test.h"):
            continue
        mod = module_of(path)
        text = path.read_text()
        stats[mod]["files"] += 1
        stats[mod]["loc"] += len(text.splitlines())
        for match in re.finditer(r"^(?:class|struct)\s+(\w+)", text, re.M):
            stats[mod]["classes"].add(match.group(1))
    for path in sorted(SRC.rglob("*.cc")):
        if path.name.endswith("_test.cc"):
            continue
        stats[module_of(path)]["loc"] += len(path.read_text().splitlines())

    rows = []
    for mod, data in sorted(stats.items()):
        hits = sorted(c for c in data["classes"] if c in tokens)
        rows.append({"module": mod, "files": data["files"], "loc": data["loc"],
                     "classes": len(data["classes"]), "linked_classes": hits})
    live = [r for r in rows if r["linked_classes"]]
    dormant = [r for r in rows if not r["linked_classes"]]
    print(f"modules: {len(rows)}  live: {len(live)}  dormant: {len(dormant)}")
    print(f"total overlay lines: {sum(r['loc'] for r in rows)}  "
          f"lines in live modules: {sum(r['loc'] for r in live)}")
    print("\nLIVE (a class name appears in the linked binaries):")
    for r in live:
        print(f"  {r['module']:38s} {r['loc']:6d} loc  {', '.join(r['linked_classes'])}")
    print("\nDORMANT (compiled, never referenced by a call site):")
    for r in dormant:
        print(f"  {r['module']:38s} {r['loc']:6d} loc  {r['classes']} classes")

main()
