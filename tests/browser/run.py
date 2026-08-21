#!/usr/bin/env python3
"""Browser core smoke suite (roadmap item 74, Browser Core row).

    python3 tests/browser/run.py --browser /path/to/chrome     # run the checks
    python3 tests/browser/run.py --selftest                    # no browser needed

Five checks, each one driving a *real running browser binary* rather than a
mock: launch, navigation, tabs, downloads, profiles.

Why this exists and why it looks like this:

* Until Bedrock's own binary is built, the same checks run against any
  Chromium-family binary. That is the point: the harness is proven now, and the
  day `out/Release/bedrock` exists it is one `--browser` argument away.
* Everything is served from a local `http.server`. No test touches the
  internet.
* Pages report their own result by POSTing to `/report` on the local server.
  `--dump-dom` hangs forever in current Chrome-for-Testing new-headless builds,
  and this keeps the runner free of any CDP client dependency (item 77).
* Downloads are steered with a profile `Preferences` file instead of a CDP
  session -- fewer moving parts, and it exercises the same preference the UI
  writes.
"""

from __future__ import annotations

import argparse
import http.server
import json
import pathlib
import socket
import subprocess
import sys
import tempfile
import threading
import time

HERE = pathlib.Path(__file__).resolve().parent
FIXTURES = HERE / "fixtures"


# --------------------------------------------------------------------------- serving


class Handler(http.server.SimpleHTTPRequestHandler):
    """Serves `fixtures/`, and collects what the pages report back.

    `--dump-dom` never returns in current Chrome-for-Testing new-headless
    builds, so the fixtures POST their result to /report on this server and the
    runner waits for it. Same machine, loopback only, no client library.
    """

    reports: list[str] = []
    paths: list[str] = []
    lock = threading.Lock()

    def translate_path(self, path: str) -> str:  # noqa: D102 - stdlib override
        return str(FIXTURES / path.split("?", 1)[0].lstrip("/"))

    def do_GET(self) -> None:  # noqa: N802 - stdlib override
        with Handler.lock:
            Handler.paths.append(self.path)
        super().do_GET()

    def do_POST(self) -> None:  # noqa: N802 - stdlib override
        length = int(self.headers.get("Content-Length", "0"))
        with Handler.lock:
            Handler.reports.append(self.rfile.read(length).decode("utf-8", "replace"))
        self.send_response(204)
        self.end_headers()

    def log_message(self, *args) -> None:
        pass


def reset_server_state() -> None:
    with Handler.lock:
        Handler.reports.clear()
        Handler.paths.clear()


def collected(seconds: float) -> list[str]:
    deadline = time.time() + seconds
    while time.time() < deadline:
        with Handler.lock:
            if Handler.reports:
                return list(Handler.reports)
        time.sleep(0.2)
    return []


def seen_paths() -> list[str]:
    with Handler.lock:
        return list(Handler.paths)


def serve() -> tuple[http.server.ThreadingHTTPServer, str]:
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    server = http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server, f"http://127.0.0.1:{port}"


# --------------------------------------------------------------------------- driving


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


def launch(browser: str, url: str, profile: pathlib.Path, extra: list[str] | None = None
           ) -> subprocess.Popen:
    return subprocess.Popen(
        [browser, *BASE_FLAGS, f"--user-data-dir={profile}", *(extra or []), url],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def stop(process: subprocess.Popen) -> None:
    process.terminate()
    try:
        process.wait(timeout=20)
    except subprocess.TimeoutExpired:
        process.kill()


def load_and_report(browser: str, url: str, profile: pathlib.Path,
                    seconds: float = 25, settle: float = 0) -> str:
    """Open one page and return what it POSTed to /report ("" on timeout).

    `settle` keeps the browser alive after the report: profile state (cookies,
    localStorage) is flushed asynchronously, so killing the process the
    millisecond a page reports loses writes that a real session would keep.
    """
    reset_server_state()
    process = launch(browser, url, profile)
    try:
        reports = collected(seconds)
        if reports and settle:
            time.sleep(settle)
    finally:
        stop(process)
    return reports[0].strip() if reports else ""


# --------------------------------------------------------------------------- checks


def check_launch(browser: str, origin: str, workdir: pathlib.Path) -> str:
    """The binary starts and reports a version."""
    done = subprocess.run([browser, "--version"], capture_output=True, text=True,
                          timeout=60)
    if done.returncode != 0:
        return f"--version exited {done.returncode}"
    if not done.stdout.strip():
        return "--version printed nothing"
    return ""


def check_navigation(browser: str, origin: str, workdir: pathlib.Path) -> str:
    """A page loads, scripts run, and the loaded document is the requested one."""
    reported = load_and_report(browser, f"{origin}/page.html?probe=navigation",
                               workdir / "nav")
    if "probe=navigation" not in reported:
        return f"page did not report its own URL (got {reported[:120]!r})"
    if "bedrock-test-page" not in reported:
        return f"unexpected document title in report: {reported[:120]!r}"
    return ""


def check_tabs(browser: str, origin: str, workdir: pathlib.Path) -> str:
    """A page opens a second tab and both tabs load and report."""
    reset_server_state()
    profile = workdir / "tabs"
    process = launch(browser, f"{origin}/tabs.html", profile,
                     extra=["--disable-popup-blocking"])
    reports: list[str] = []
    try:
        deadline = time.time() + 40
        while time.time() < deadline:
            with Handler.lock:
                reports = list(Handler.reports)
            if len(reports) >= 2:
                break
            time.sleep(0.25)
    finally:
        stop(process)
    tabs = {name for name in ("tab=1", "tab=2")
            if any(name in report for report in reports)}
    if tabs != {"tab=1", "tab=2"}:
        return (f"both tabs did not load and report: saw {sorted(tabs)} "
                f"in {len(reports)} report(s); requested {seen_paths()}")
    return ""


def check_downloads(browser: str, origin: str, workdir: pathlib.Path) -> str:
    """A download started by a page lands in the configured directory."""
    profile = workdir / "downloads"
    target = workdir / "downloaded"
    target.mkdir(parents=True, exist_ok=True)
    (profile / "Default").mkdir(parents=True, exist_ok=True)
    (profile / "Default" / "Preferences").write_text(json.dumps({
        "download": {"default_directory": str(target), "prompt_for_download": False},
        "savefile": {"default_directory": str(target)},
    }))
    reset_server_state()
    process = launch(browser, f"{origin}/download.html", profile)
    try:
        deadline = time.time() + 40
        files: list[pathlib.Path] = []
        while time.time() < deadline:
            files = [p for p in target.iterdir() if not p.name.endswith(".crdownload")]
            if files:
                break
            time.sleep(0.5)
    finally:
        stop(process)
    if not files:
        return f"no file appeared in {target}"
    if files[0].read_text().strip() != "bedrock-download-payload":
        return f"downloaded file has unexpected content: {files[0].name}"
    return ""


def check_profiles(browser: str, origin: str, workdir: pathlib.Path) -> str:
    """State written in one profile is visible on reuse and absent in a fresh one."""
    first = workdir / "profile-a"
    second = workdir / "profile-b"
    # 8s: measured. Chrome flushes localStorage lazily; at 3s the value was
    # still lost when the process was stopped, at 8s it survives.
    write = load_and_report(browser, f"{origin}/storage.html?write=alpha", first,
                            settle=8)
    if "written:alpha" not in write:
        return f"first profile did not write state (got {write[:80]!r})"
    reread = load_and_report(browser, f"{origin}/storage.html", first)
    if "read:alpha" not in reread:
        return f"state did not survive within the same profile (got {reread[:80]!r})"
    other = load_and_report(browser, f"{origin}/storage.html", second)
    if "read:alpha" in other:
        return f"a fresh profile could read the other profile's state: {other[:80]!r}"
    if "read:" not in other:
        return f"fresh profile did not report at all (got {other[:80]!r})"
    return ""


CHECKS = {
    "launch": check_launch,
    "navigation": check_navigation,
    "tabs": check_tabs,
    "downloads": check_downloads,
    "profiles": check_profiles,
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", help="path to the browser binary under test")
    parser.add_argument("--only", choices=sorted(CHECKS))
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        assert FIXTURES.is_dir(), "fixtures directory missing"
        assert set(CHECKS) == {"launch", "navigation", "tabs", "downloads", "profiles"}
        for fixture in ("page.html", "download.html", "storage.html", "tabs.html"):
            assert (FIXTURES / fixture).exists(), f"missing fixture {fixture}"
        assert not any("http://" in path.read_text()
                       for path in FIXTURES.glob("*.html")), \
            "fixtures must not reference absolute URLs"
        print(f"selftest OK ({len(CHECKS)} checks, "
              f"{len(list(FIXTURES.glob('*.html')))} fixtures)")
        return 0

    if not args.browser:
        parser.error("--browser is required (path to the binary to exercise)")

    server, origin = serve()
    failures = 0
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as tmp:
        workdir = pathlib.Path(tmp)
        for name, check in CHECKS.items():
            if args.only and name != args.only:
                continue
            started = time.time()
            try:
                problem = check(args.browser, origin, workdir)
            except Exception as error:  # a crashed check is a failed check
                problem = f"{type(error).__name__}: {error}"
            failures += bool(problem)
            print(f"{'FAIL' if problem else 'PASS':4}  {name:11} "
                  f"{time.time() - started:5.1f}s  {problem}")
    server.shutdown()
    print(f"\n{len(CHECKS) - failures}/{len(CHECKS)} browser core checks pass")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
