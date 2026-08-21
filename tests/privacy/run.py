#!/usr/bin/env python3
"""Privacy regression suite runner (roadmap item 75).

    python3 tests/privacy/run.py --browser /path/to/chrome            # verify
    python3 tests/privacy/run.py --browser /path/to/chrome --baseline out.json
    python3 tests/privacy/run.py --selftest

Serves `fixtures/` from two local origins, drives a real browser binary at each
scenario, and compares what the page could observe against
`expectations.json`.

Design decisions worth knowing before changing this:

* **Everything is local.** Two `http.server` instances on 127.0.0.1, two
  hostnames so cross-site cases are genuinely cross-site. Nothing is requested
  from the internet and no measured value ever leaves the machine — a privacy
  suite that reports to a third party has already broken its own rule (item 75).
* **The server records request headers**, because half the answers are not
  visible to JavaScript: the `Referer` a tracker receives, the `Cookie` sent to
  a third-party frame, the `Sec-CH-*` hints sent without being asked.
* **Failures against stock Chromium are expected.** Run with `--baseline` to
  record what an unprotected browser exposes; that file is the "before" column,
  and it is checked in, so a claim that Bedrock changes something can be
  compared against a measurement instead of an assumption.
"""

from __future__ import annotations

import argparse
import http.server
import json
import pathlib
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse

HERE = pathlib.Path(__file__).resolve().parent
FIXTURES = HERE / "fixtures"
EXPECTATIONS = HERE / "expectations.json"
BASE_FLAGS = [
    "--headless=new",
    "--no-sandbox",              # sandboxed CI containers
    "--disable-gpu",
    # Startup must not depend on the network. Without these, a machine with no
    # internet access spends ~90s in GCM/component-update retries before the
    # first paint, and every scenario looks like a timeout instead of a result.
    "--no-first-run",
    "--no-default-browser-check",
    "--disable-background-networking",
    "--disable-component-update",
    "--disable-sync",
    "--disable-client-side-phishing-detection",
    "--disable-domain-reliability",
    "--metrics-recording-only",
]



class RecordingHandler(http.server.SimpleHTTPRequestHandler):
    """Static file server that keeps every request's headers."""

    requests: list[dict] = []
    lock = threading.Lock()

    def translate_path(self, path: str) -> str:  # noqa: D102 - stdlib override
        clean = urllib.parse.urlparse(path).path.lstrip("/")
        return str(FIXTURES / clean)

    reports: list[dict] = []

    def do_POST(self) -> None:  # noqa: N802 - stdlib override
        """Fixtures POST their measurements here; this is the result channel."""
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", "replace")
        try:
            payload = json.loads(body)
        except json.JSONDecodeError as error:
            payload = {"_error": f"fixture posted invalid JSON: {error}"}
        with RecordingHandler.lock:
            RecordingHandler.reports.append(payload)
        self.send_response(204)
        self.end_headers()

    def do_GET(self) -> None:  # noqa: N802 - stdlib override
        with RecordingHandler.lock:
            RecordingHandler.requests.append({
                "host": self.headers.get("Host", ""),
                "path": self.path,
                "referer": self.headers.get("Referer"),
                "cookie": self.headers.get("Cookie"),
                "accept_language": self.headers.get("Accept-Language"),
                "user_agent": self.headers.get("User-Agent"),
                "client_hints": {
                    name: value
                    for name, value in self.headers.items()
                    if name.lower().startswith("sec-ch-")
                },
                "sec_gpc": self.headers.get("Sec-GPC"),
                "dnt": self.headers.get("DNT"),
            })
        if urllib.parse.urlparse(self.path).path == "/probe.gif":
            self.send_response(200)
            self.send_header("Content-Type", "image/gif")
            self.end_headers()
            self.wfile.write(b"GIF89a")
            return
        super().do_GET()

    def log_message(self, *args) -> None:  # keep the output readable
        pass


def free_port() -> int:
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def serve() -> tuple[http.server.ThreadingHTTPServer, int]:
    port = free_port()
    server = http.server.ThreadingHTTPServer(("127.0.0.1", port), RecordingHandler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server, port


def run_page(browser: str, url: str, profile: pathlib.Path, seconds: int = 20) -> dict:
    """Load one page in a real browser and return the JSON the fixture posted.

    The browser is started in the background and killed as soon as its report
    arrives -- `--dump-dom` hangs forever in current new-headless builds, so the
    page reporting to the local server is the transport (see fixtures/_harness.js).
    """
    command = [
        browser,
        *BASE_FLAGS,
        f"--user-data-dir={profile}",
        url,
    ]
    with RecordingHandler.lock:
        RecordingHandler.reports.clear()
    process = subprocess.Popen(command, stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL)
    try:
        deadline = time.time() + seconds
        while time.time() < deadline:
            with RecordingHandler.lock:
                if RecordingHandler.reports:
                    return RecordingHandler.reports[0]
            if process.poll() is not None:
                break
            time.sleep(0.25)
        return {"_error": f"fixture posted no result within {seconds}s"}
    finally:
        process.terminate()
        try:
            process.wait(timeout=20)
        except subprocess.TimeoutExpired:
            process.kill()


def lookup(values: dict, path: str):
    current = values
    for part in path.split("."):
        if isinstance(current, list):
            try:
                current = current[int(part)]
                continue
            except (ValueError, IndexError):
                return None
        if not isinstance(current, dict) or part not in current:
            return None
        current = current[part]
    return current


def evaluate(observed: dict, checks: dict) -> list[tuple[str, bool, object]]:
    """Each check is `path: expected`. `expected` may be a value, or
    {"not": v} / {"in": [...]} / {"absent": true} / {"max_len": n}."""
    outcomes = []
    for path, expected in checks.items():
        actual = lookup(observed, path)
        if isinstance(expected, dict):
            if "not" in expected:
                ok = actual != expected["not"]
            elif "in" in expected:
                ok = actual in expected["in"]
            elif "absent" in expected:
                ok = (actual in (None, [], "", "undefined")) == bool(expected["absent"])
            elif "max_len" in expected:
                ok = actual is not None and len(actual) <= expected["max_len"]
            elif "max" in expected:
                ok = isinstance(actual, (int, float)) and actual <= expected["max"]
            else:
                ok = False
        else:
            ok = actual == expected
        outcomes.append((path, ok, actual))
    return outcomes


def scenario_url(scenario: dict, origin_a: str, origin_b: str) -> str:
    return (
        scenario["url"]
        .replace("{A}", origin_a)
        .replace("{B}", origin_b)
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", help="path to the browser binary under test")
    parser.add_argument("--baseline", type=pathlib.Path,
                        help="record observations to this file instead of failing")
    parser.add_argument("--only", help="run one scenario by id")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    expectations = json.loads(EXPECTATIONS.read_text(encoding="utf-8"))

    if args.selftest:
        assert lookup({"a": {"b": [1, 2]}}, "a.b.1") == 2
        assert evaluate({"x": 1}, {"x": 1})[0][1] is True
        assert evaluate({"x": 1}, {"x": {"not": 1}})[0][1] is False
        assert evaluate({"x": []}, {"x": {"absent": True}})[0][1] is True
        assert evaluate({}, {"missing": {"absent": True}})[0][1] is True
        ids = {s["id"] for s in expectations["scenarios"]}
        # storage_isolation.html is loaded by the top-level fixture, not directly.
        embedded = {"storage_isolation.html"}
        missing = [f for f in FIXTURES.glob("*.html")
                   if f.name not in embedded
                   and not any(f.name in s["url"] for s in expectations["scenarios"])]
        assert not missing, f"fixtures nobody runs: {[f.name for f in missing]}"
        assert len(ids) == len(expectations["scenarios"]), "duplicate scenario id"
        print(f"selftest OK ({len(ids)} scenarios, {len(list(FIXTURES.glob('*.html')))} fixtures)")
        return 0

    if not args.browser:
        parser.error("--browser is required (path to the binary to measure)")

    server_a, port_a = serve()
    server_b, port_b = serve()
    # Two different hostnames for the same loopback interface: same machine,
    # genuinely different sites as far as the browser is concerned.
    origin_a = f"http://127.0.0.1:{port_a}"
    origin_b = f"http://localhost:{port_b}"

    results = {}
    failures = 0
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as profile_root:
        for scenario in expectations["scenarios"]:
            if args.only and scenario["id"] != args.only:
                continue
            RecordingHandler.requests.clear()
            profile = pathlib.Path(profile_root) / scenario["id"]
            observed = run_page(args.browser, scenario_url(scenario, origin_a, origin_b), profile)
            with RecordingHandler.lock:
                requests = list(RecordingHandler.requests)
            record = {"observed": observed, "requests": requests}

            checks = evaluate(observed, scenario.get("checks", {}))
            record["checks"] = [
                {"path": path, "pass": ok, "actual": actual} for path, ok, actual in checks
            ]
            failed = [c for c in checks if not c[1]]
            results[scenario["id"]] = record

            status = "PASS" if not failed and "_error" not in observed else "FAIL"
            if status == "FAIL" and not args.baseline:
                failures += 1
            print(f"{status:4}  {scenario['id']:22} {scenario['requirement']}")
            for path, ok, actual in checks:
                if not ok:
                    print(f"        {path} = {actual!r}")
            if "_error" in observed:
                print(f"        {observed['_error']}")

    server_a.shutdown()
    server_b.shutdown()

    if args.baseline:
        args.baseline.write_text(json.dumps(results, indent=1, sort_keys=True) + "\n")
        print(f"\nbaseline written to {args.baseline}")
        return 0

    print(f"\n{len(results) - failures}/{len(results)} scenarios pass")
    return 1 if failures else 0


if __name__ == "__main__":
    if not shutil.which("true"):  # pragma: no cover - sanity only
        sys.exit("unexpected environment")
    sys.exit(main())
