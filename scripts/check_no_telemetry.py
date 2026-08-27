#!/usr/bin/env python3
"""Fail if anything that reports home appears in the tree, or if the build
arguments that disable Chromium's reporting are missing.

Run: python3 scripts/check_no_telemetry.py

Roadmap item 39 is "zero telemetry by default". A policy class states that; this
scanner is what keeps it true when someone adds a metric "just to see how often
this happens". It checks two things:

  1. Bedrock's own sources contain no reporting machinery and no analytics or
     crash-upload hosts.
  2. build/args/bedrock-release.gn still disables Chromium's own reporting.

Discussion of telemetry in docs and comments is fine — the scan is over code.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SOURCES = REPO / "src_overrides"
RELEASE_ARGS = REPO / "build" / "args" / "bedrock-release.gn"

# Reporting machinery that must not appear in Bedrock code.
BANNED_SYMBOLS = [
    "UMA_HISTOGRAM", "UmaHistogram", "base::UmaHistogram",
    "RecordAction", "metrics::MetricsService", "MetricsLogUploader",
    "crash_reporter::", "crashpad::CrashReportDatabase", "breakpad",
    "variations::", "field_trial_config", "RapporService",
    "StructuredMetrics", "UkmRecorder", "SendUsageStats",
]

# Hosts that would mean data leaving the machine to a vendor.
BANNED_HOSTS = [
    "google-analytics.com", "www.google-analytics.com",     "clients2.google.com", "clients4.google.com", "update.googleapis.com",
    "sentry.io", "bugsnag.com", "mixpanel.com", "segment.io", "amplitude.com",
    "plausible.io", "matomo.cloud", "posthog.com",
]

# GN args that must stay off in the release configuration.
REQUIRED_ARGS = {
    "enable_reporting": "false",
    "safe_browsing_mode": "0",
    "use_official_google_api_keys": "false",
}

ALLOWED_STRING_CONTEXT = re.compile(r"^\s*(//|\*|/\*)")

# A filter-list rule that names a tracker host is the opposite of phoning home:
# it is the instruction not to contact it. Only this exact shape is allowed —
# a quoted rule, domain-anchored, party-scoped, nothing else on the line — so
# "https://google-analytics.com/collect" is still a failure, and so is a bare
# hostname in a URL, a header or a config value.
ALLOWED_BLOCK_RULE = re.compile(
    r'^\s*"(?:!\s[^"]*|\|\|[a-z0-9.\-]+\^\$[a-z\-]+)\\n"\s*$')


def scan_sources() -> list[str]:
    errors = []
    for path in sorted(SOURCES.rglob("*")):
        if path.suffix not in {".cc", ".h", ".json", ".gn", ".js", ".ts"}:
            continue
        if path.name.endswith("_test.cc"):
            # Tests deliberately contain tracker hostnames and fake endpoints;
            # they are inputs, not calls. The shipped code is what matters.
            continue
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if ALLOWED_STRING_CONTEXT.match(line):
                continue  # a comment may name what we do not do
            if ALLOWED_BLOCK_RULE.match(line):
                continue  # a block rule names a host in order to refuse it
            for symbol in BANNED_SYMBOLS:
                if symbol in line:
                    errors.append(
                        f"{path.relative_to(REPO)}:{number}: reporting machinery "
                        f"{symbol!r} — see docs/design/041, item 39")
            for host in BANNED_HOSTS:
                if host in line:
                    errors.append(
                        f"{path.relative_to(REPO)}:{number}: phones home to {host!r}")
    return errors


def scan_release_args() -> list[str]:
    errors = []
    text = RELEASE_ARGS.read_text()
    for key, expected in REQUIRED_ARGS.items():
        match = re.search(rf"^{re.escape(key)}\s*=\s*(\S+)", text, re.M)
        if not match:
            errors.append(f"build/args/bedrock-release.gn: {key} is not set")
        elif match.group(1) != expected:
            errors.append(
                f"build/args/bedrock-release.gn: {key} = {match.group(1)}, "
                f"must be {expected}")
    return errors


def main() -> int:
    errors = scan_sources() + scan_release_args()
    if errors:
        print("telemetry check FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(f"telemetry OK: no reporting machinery, "
          f"{len(REQUIRED_ARGS)} release args verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
