# Test matrix

Roadmap item 74. Every case the roadmap asks for is listed in
[`matrix.json`](matrix.json) with the runner that executes it and an honest
status. `scripts/check_test_matrix.py` fails the build if a case is missing, if
a named runner does not exist, or if this document and the JSON disagree — the
matrix cannot rot into a wish list.

## What "status" means

| Status | Meaning |
| --- | --- |
| `running` | Executes today on a machine with no Chromium checkout, either as a host test (`./scripts/run_host_tests.sh`) or against a real browser binary. |
| `needs-build` | The harness exists and produces measurements today, but the pass/fail verdict is only meaningful once a Bedrock binary exists. |
| `needs-network` | Requires a live external service (only the Tor network). Never run in CI by default. |

## How to run each layer

```bash
./scripts/run_host_tests.sh                                  # every host test + every gate
python3 tests/browser/run.py  --browser /path/to/chrome      # browser core, real binary
python3 tests/privacy/run.py  --browser /path/to/chrome      # privacy regression suite
python3 tests/browser/run.py  --selftest                     # harness check, no browser
python3 tests/privacy/run.py  --selftest
```

`--browser` accepts any Chromium-family binary. Until Bedrock's own binary
exists, running the suites against stock Chromium is what proves the harness
works — and, for the privacy suite, what produced
[`tests/privacy/baseline-chromium.json`](privacy/baseline-chromium.json), the
"before" column an eventual Bedrock result is compared against.

Both suites serve their fixtures from a local `http.server` on 127.0.0.1 and
receive results as a POST from the page back to that same server. Nothing is
requested from the internet, and no measured value leaves the machine.

## Coverage

| Area | Case | Kind | Executes today |
| --- | --- | --- | --- |
| Browser Core | launch | browser | yes |
| Browser Core | navigation | browser | yes |
| Browser Core | tabs | browser | yes |
| Browser Core | downloads | browser | yes |
| Browser Core | profiles | browser | yes |
| Privacy | third-party cookies | fixture | measured, verdict needs a build |
| Privacy | trackers | host | yes |
| Privacy | fingerprint APIs | fixture | measured, verdict needs a build |
| Privacy | referrer | fixture | measured, verdict needs a build |
| Privacy | query stripping | fixture | measured, verdict needs a build |
| Privacy | storage partition | fixture | measured, verdict needs a build |
| Privacy | WebRTC | fixture | measured, verdict needs a build |
| Privacy | HTTPS | host | yes |
| Search | Google | host | yes |
| Search | DuckDuckGo | host | yes |
| Search | custom search providers | host | yes |
| Extensions | installation | host | yes |
| Extensions | permissions | host | yes |
| Extensions | execution | browser | no — needs a build |
| Extensions | isolation | host | yes |
| Tor | proxy routing | browser | no — needs a tor daemon |
| Tor | identity reset | host | yes |
| Tor | DNS behavior | host | yes |
| UI | themes | host | yes |
| UI | accessibility | host | yes |
| UI | keyboard navigation | host | yes |

## The three cases that cannot run yet, and exactly what unblocks them

**Extensions → execution.** Needs a browser that can load an unpacked
extension. Once `out/Release/bedrock` exists:

```bash
bedrock --headless=new --user-data-dir=$(mktemp -d) \
        --load-extension=tests/extensions/fixtures/probe \
        http://127.0.0.1:PORT/page.html
```

The probe extension's content script writes a marker into the page and the
existing `/report` transport carries it back; the assertion is that the marker
appears on an allowed host and does not appear on a host the manifest does not
list. Nothing about that harness needs new infrastructure — only the binary.

**Tor → proxy routing.** Needs a running `tor` daemon exposing a SOCKS port.
Bedrock does not bundle one yet (open decision, `.ai/memory/STATE.md`), and a
test that silently reaches the real Tor network from CI is the wrong default.
When the decision lands, the check is: with the proxy configured, every request
leaves through the SOCKS port and no DNS query is made locally — the second half
is already asserted without the network in
`privacy/network/dns_settings_test.cc`.

**Privacy fixtures.** They run today and record what the browser exposes; what
is missing is a Bedrock binary whose behaviour differs from the baseline. The
expectations in `tests/privacy/expectations.json` are written against Bedrock's
intended behaviour, so they fail against stock Chromium on purpose.

## Manual checks that stay manual

Screen-reader output (NVDA, Orca, VoiceOver), high-contrast and forced-colors
modes on real desktops, and Windows/Linux installer behaviour are verified by
hand before a release; see `docs/RELEASES.md`. Automating a screen reader would
test the automation, not the experience.
