# Security testing

Static analysis, sanitizers, fuzzing, unit / integration / browser / regression /
crash / network-privacy tests — what runs where, and what it covers.

**Roadmap item 43.** What runs, where, and what it covers. Checked by
`scripts/check_security_testing.py`, so this table cannot quietly become fiction.

## What runs on every commit (CI, `scripts/run_host_tests.sh`)

| Kind | How |
| --- | --- |
| Unit tests | one `*_test.cc` per component, dependency-free, own `main()` — 34 binaries |
| Regression tests | every fixed bug becomes an assertion in the component's test; assertions are sentences, so the reason survives |
| Fuzz smoke | every harness in `src_overrides/bedrock/fuzz/` is compiled and replayed against the seed corpus, ~860 inputs each, including truncations and byte flips |
| Performance budgets | `perf_budgets_test.cc` measures and fails on regression (`docs/performance/BUDGETS.md`) |
| Static rules | provenance, fingerprinting docs, UI style, catalog, telemetry, open-source and SBOM gates |
| Network privacy tests | DNS, WebRTC, HTTPS, storage-isolation and blocking-pipeline tests assert *no* leak paths: system resolver default, no Bedrock resolver, local addresses withheld, no global certificate bypass |

The compiler settings are part of the testing: `-Wall -Wextra -Werror -std=c++17`.

## What runs on a real build

| Kind | Configuration | Cadence |
| --- | --- | --- |
| ASan + UBSan | `build/args/bedrock-asan.gn` (`is_ubsan_no_recover = true`) | nightly |
| MSan | `build/args/bedrock-msan.gn`, instrumented libc++ | nightly |
| TSan | `build/args/bedrock-tsan.gn` | weekly (slow) |
| libFuzzer campaigns | `build/args/bedrock-fuzz.gn`, ASan + `optimize_for_fuzzing` | continuous, corpus committed on new coverage |
| Browser tests | Chromium's `browser_tests` plus Bedrock cases for shields, profiles, private windows | per release |
| Integration tests | full pipeline against a local test server: blocking, storage keys, DNS modes, WebRTC policy | per release |
| Crash tests | replay of every crash-corpus input; a crash file never leaves the machine (item 39) | per release |
| Static analysis | `clang-tidy` with Chromium's config, plus `gn check` for layering | per release |

Nightly and per-release runs need a Chromium checkout, so they are documented
here rather than pretended to run in CI today. `scripts/check_security_testing.py`
verifies the configurations still exist and still say what they should.

## Areas item 43 names, and where they are covered

| Area | Tests |
| --- | --- |
| Renderer-facing policy | `privacy/fingerprint_policy_test`, `privacy/privacy_policy_test` |
| Networking | `net/dns_settings_test`, `net/https_policy_test`, `net/webrtc_policy_test`, `net/request_headers_test`, `fuzz/request_headers_fuzzer` |
| URL parser | `omnibox/input_parser_test`, `fuzz/omnibox_input_fuzzer` |
| Extension system | `extensions/extension_registry_test`, `catalog/extension_catalog_test` |
| Content blocking | `blocking/filter_engine_test`, `blocking/blocking_pipeline_test`, `blocking/tracker_heuristic_test`, `fuzz/filter_list_fuzzer` |
| Permissions | `extensions/extension_registry_test` (disclosure, escalation review) |
| Privacy APIs | `privacy/protection_controller_test`, `privacy/privacy_policy_test`, `privacy/security_levels_test` |
| Storage isolation | `net/storage_isolation_test` |
| Downloads | `downloads/download_manager_test`, `fuzz/download_name_fuzzer` |
| Update path | `update/update_provider_test` |

## Fuzzing

Harnesses live in `src_overrides/bedrock/fuzz/` and target the code that eats
untrusted input: filter lists (a user can subscribe to any list on the
internet), omnibox input (every keystroke, plus pasted data), download names and
MIME types (attacker-chosen by definition), bookmark import HTML, and the
outgoing-header policy (the referring URL and the declared `Referrer-Policy`
both come from the page, so a crash there is remotely triggerable by any site).

Each file is a normal libFuzzer entry point *and* compiles with
`-DBEDROCK_FUZZ_SMOKE` into a deterministic replay binary. That dual build is
the point: the usual fuzzing failure is not a missing harness but one that
stopped compiling months ago and nobody noticed.

New crash inputs are committed to the corpus with a regression assertion in the
matching component test, so a fixed crash stays fixed even if the fuzzer never
runs again.
