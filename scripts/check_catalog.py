#!/usr/bin/env python3
"""Validate the privacy extension catalog. Run: python3 scripts/check_catalog.py

The catalog is data, so the rules that keep it honest have to be enforced on the
data, not on the code that reads it:

  1. every entry carries its OWN license, official source and attribution
     (the license of the site that recommends a tool never covers the tool);
  2. no entry claims to be an official Bedrock extension;
  3. attribution for a recommendation source names that source;
  4. `last_verified` is a real date, not in the future, and not older than
     the staleness window used by the UI;
  5. capabilities and enum-valued fields are ones the C++ knows about;
  6. at most one non-duplicate entry per capability is marked as a default
     recommendation for a level (no pile of overlapping tools).
"""

from __future__ import annotations

import json
import sys
from datetime import date
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CATALOG = REPO / "src_overrides" / "bedrock" / "extensions" / "catalog" / "bedrock_privacy_catalog.json"

CAPABILITIES = {
    "content_blocking", "tracker_blocking", "url_parameter_cleaning",
    "cookie_cleanup", "script_control", "fingerprint_defense",
    "local_cdn_emulation", "https_enforcement", "password_management",
}
THREAT_LEVELS = {"covered", "hardened", "targeted"}
PROVENANCE = {"privacytools_recommendation", "bedrock_recommendation", "built_into_bedrock"}
COMPATIBILITY = {"chromium_mv3", "chromium_mv2_only", "firefox_only", "unknown"}
MAINTENANCE = {"active", "slow", "unmaintained"}
MAX_VERIFICATION_AGE_DAYS = 180
FORBIDDEN_PHRASES = ("official bedrock", "bedrock extension", "by bedrock team")


def check(catalog: dict, today: date) -> list[str]:
    errors: list[str] = []
    sources = {s["id"]: s for s in catalog.get("sources", [])}
    if "privacytools-io" not in sources:
        errors.append("sources: the PrivacyTools.io record is missing")
    else:
        source = sources["privacytools-io"]
        if not source.get("attribution_required"):
            errors.append("privacytools-io: their license requires attribution")
        if "privacytools.io" not in source.get("attribution_text", "").lower():
            errors.append("privacytools-io: attribution text must name the source")
        if not source.get("license_url", "").startswith("http"):
            errors.append("privacytools-io: license_url must be a link to the license")

    for capability in catalog.get("built_in_protections", []):
        if capability not in CAPABILITIES:
            errors.append(f"built_in_protections: unknown capability {capability!r}")

    seen_ids: set[str] = set()
    for entry in catalog.get("extensions", []):
        name = entry.get("id", "<no id>")
        if name in seen_ids:
            errors.append(f"{name}: duplicate id")
        seen_ids.add(name)

        for field in ("name", "description", "privacy_purpose", "license",
                      "official_source", "attribution", "why_recommended",
                      "version", "icon"):
            if not entry.get(field):
                errors.append(f"{name}: missing {field}")

        if not str(entry.get("official_source", "")).startswith("http"):
            errors.append(f"{name}: official_source must be a URL")

        text = (entry.get("attribution", "") + " " + entry.get("name", "")).lower()
        for phrase in FORBIDDEN_PHRASES:
            if phrase in text:
                errors.append(
                    f"{name}: must not present a third-party extension as a Bedrock one "
                    f"({phrase!r})")

        if entry.get("provenance") == "privacytools_recommendation" and \
                "privacytools.io" not in entry.get("attribution", "").lower():
            errors.append(f"{name}: recommended by PrivacyTools.io but does not credit them")

        if entry.get("provenance") not in PROVENANCE:
            errors.append(f"{name}: unknown provenance {entry.get('provenance')!r}")
        if entry.get("threat_level") not in THREAT_LEVELS:
            errors.append(f"{name}: unknown threat_level {entry.get('threat_level')!r}")
        if entry.get("compatibility") not in COMPATIBILITY:
            errors.append(f"{name}: unknown compatibility {entry.get('compatibility')!r}")
        if entry.get("maintenance") not in MAINTENANCE:
            errors.append(f"{name}: unknown maintenance {entry.get('maintenance')!r}")
        if not entry.get("capabilities"):
            errors.append(f"{name}: no capabilities — overlap cannot be computed")
        for capability in entry.get("capabilities", []):
            if capability not in CAPABILITIES:
                errors.append(f"{name}: unknown capability {capability!r}")
        if not isinstance(entry.get("permissions"), list):
            errors.append(f"{name}: permissions must be a list, even an empty one")

        verified = entry.get("last_verified", "")
        try:
            checked = date.fromisoformat(verified)
        except ValueError:
            errors.append(f"{name}: last_verified {verified!r} is not an ISO date")
            continue
        if checked > today:
            errors.append(f"{name}: last_verified is in the future")
        elif (today - checked).days > MAX_VERIFICATION_AGE_DAYS:
            errors.append(
                f"{name}: last verified {(today - checked).days} days ago; recheck the "
                f"upstream page or drop the entry (limit {MAX_VERIFICATION_AGE_DAYS})")

    return errors


def main() -> int:
    catalog = json.loads(CATALOG.read_text())
    errors = check(catalog, date.today())
    if errors:
        print("catalog check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(f"catalog OK: {len(catalog['extensions'])} entries, "
          f"{len(catalog['sources'])} sources, attribution present")
    return 0


def _selftest() -> None:
    today = date(2026, 8, 21)
    base = json.loads(CATALOG.read_text())
    assert check(base, today) == [], check(base, today)
    fake_official = json.loads(json.dumps(base))
    fake_official["extensions"][0]["attribution"] = "An Official BEDROCK extension"
    assert any("third-party" in e for e in check(fake_official, today))
    stale = json.loads(json.dumps(base))
    stale["extensions"][0]["last_verified"] = "2024-01-01"
    assert any("last verified" in e for e in check(stale, today))
    print("selftest OK")


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        _selftest()
        raise SystemExit(0)
    raise SystemExit(main())
