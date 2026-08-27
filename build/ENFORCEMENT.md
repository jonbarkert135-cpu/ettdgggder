# Enforcement record

This file exists only when a real Chromium build has been produced with the Bedrock overlay
compiled into it. Two gates read it — `scripts/check_no_fake_features.py` (a feature may not be
`Status::kEnforced` unless it is named here) and `scripts/check_phases.py` (a build-dependent phase
may not be `done` unless this file exists). Deleting it is the correct thing to do if the claim
below ever stops being true.

## Build 1 — 2026-08-22

| | |
| --- | --- |
| Chromium revision | `a96602f30358e9b5d256a0464e7e4d4bec223004` (151.0.7922.173), matches [`chromium.pin`](chromium.pin) |
| Host | Linux x64, 17 cores, 448 GB RAM |
| Toolchain | clang from the checkout (`third_party/llvm-build/Release+Asserts/bin/clang++`) |
| GN args | stock Chromium release-like: `is_debug=false`, `symbol_level=0`, `is_component_build=true`, `use_sysroot=true`, `enable_nacl=false`, `use_remoteexec=false`, `is_chrome_branded=false`, `chrome_pgo_phase=0`, `treat_warnings_as_errors=false` |
| Stock Chromium build | 56 105 steps, 12 h 16 m 39 s, exit 0 |
| Binary | `out/Release/chrome`, 194 119 320 bytes, `--version` → `Chromium 151.0.7922.173` |
| Smoke test | `--headless=new --screenshot` on a local file rendered correctly |
| Overlay build | `//bedrock` added to `//chrome/browser:browser` `public_deps`, `gn gen` → 31 626 targets, incremental `autoninja chrome` exit 0, **53 Bedrock object files** compiled by Chromium's clang and passed to the `chrome` link |

### What this build does and does not prove

**Proves:** all 106 overlay sources compile under Chromium's own clang, its warning set, its clang
plugins and `-fno-exceptions`, and the resulting objects reach the `chrome` link line
(`out/Release/obj/chrome/chrome_initial.ninja`). The build system integration is real.

**Does not prove:** that any Bedrock code *runs*. No Chromium call site calls into `bedrock::` yet,
so the linker's `--gc-sections` drops the objects from the final binary — `nm -C out/Release/chrome`
finds no `bedrock::` symbols, and that is the expected and honest result at this stage. The first
call site arrives with phase 2 (minimal browser shell).

**Therefore: no feature is listed below.** No feature is `kEnforced`, because enforcement means a
running browser performs the protection, not that its policy object compiles.

## Build 2 — 2026-08-23 (phase 2: the overlay runs)

| | |
| --- | --- |
| Base | the Build 1 output directory, incrementally relinked — same revision, same GN args |
| Change | `patches/bedrock/integration/0001-bedrock-startup-hook.patch`: two Chromium call sites into `bedrock::integration` |
| Objects rebuilt | `browser_ui_prefs.o` (92 s), `renderer_preferences_util.o` (26 s), `chrome_browser_main.o` (60 s), `bedrock/startup.o` (1 s) |
| Link | `chrome` relinked, exit 0 (cold link 21 m 39 s, warm links 13–15 s) |
| Binary | `out/Release/chrome`, 194 170 656 bytes |
| Symbols | `nm -C out/Release/chrome \| grep bedrock::` → **17 symbols** (Build 1: none) |
| Run | `--headless=new --no-sandbox --user-data-dir=…` on a fresh profile, log `/work/build-logs/run2.log` |

Observed on stderr of that run:

```
[bedrock] Balanced Privacy: 1 of 12 shipped defaults enforced by this build
[bedrock] enforcing webrtc_privacy: webrtc.ip_handling_policy = default_public_interface_only
[bedrock] effective webrtc.ip_handling_policy = default_public_interface_only (want default_public_interface_only: match)
```

### What this build proves

The first two lines come from `RegisterBrowserUserPrefs`, where Bedrock now supplies the *default*
of `webrtc.ip_handling_policy` instead of Chromium's `kWebRTCIPHandlingDefault`. The third comes
from `UpdateFromSystemSettings`, i.e. the value a live profile really hands to the renderers — it
is a measurement of the running browser, not an echo of the overlay's own constant. WebRTC in this
build therefore does not expose local interface addresses by default, and the overlay decided that.

It also proves the reverse of Build 1's finding: with a real call site, `--gc-sections` keeps the
Bedrock objects and the code executes in the browser process.

**Still not proven:** everything else. Eleven of the twelve shipped defaults are still not wired to
Chromium (the startup plan prints each one with the reason it is blocked), no UI is built, and no
privacy subsystem beyond this pref runs. Exactly one feature is `kEnforced`.

## Build 3 — 2026-08-27 (rebuilt from a fresh checkout, with PR #52 in it)

| | |
| --- | --- |
| Why | the sandbox lost `/work/chromium`; the checkout was re-fetched at the pinned revision and everything since phase 2 (PRs up to #52) was compiled for the first time |
| Chromium revision | `a96602f30358e9b5d256a0464e7e4d4bec223004` (151.0.7922.173), matches [`chromium.pin`](chromium.pin) |
| GN args | same as Build 1, minus `enable_nacl` (the arg no longer exists in this Chromium) |
| Full build | 12 h 11 m, 55 919 / 56 297 steps, **failed on one step**: `std::abs` in `bedrock/privacy/network/request_headers.cc` is not visible in Chromium's C++ modules build |
| Fix | PR #53: the sign of `OneDecimal()` is handled by hand (the old code also printed `0.5` for a DPR of −0.05), plus two regression tests and `scripts/check_toolchain_limits.py` so a `g++`-only test run cannot hide the next one |
| Rebuild | incremental, 258 steps, 24 m 01 s, exit 0 (link 4 m 21 s) |
| Binary | `out/Release/chrome`, 194 183 904 bytes |
| Symbols | `nm -C chrome \| grep bedrock::` → **18 symbols** (`integration::*`, `settings::FactoryDefaults`, `crypto::ToHex`) |
| Run | headless with a DevTools port; `Runtime.evaluate` returned the rendered text and a screenshot, so the binary really renders and runs JavaScript |

### What this build proves, and what it does not

It proves the overlay as it stands today — 20 PRs after Build 2 — still compiles and links inside
Chromium, and that the resulting browser starts, renders and enforces the one wired default:

```
[bedrock] Balanced Privacy: 1 of 12 shipped defaults enforced by this build
[bedrock] enforcing webrtc_privacy: webrtc.ip_handling_policy = default_public_interface_only
[bedrock] effective webrtc.ip_handling_policy = default_public_interface_only (want default_public_interface_only: match)
```

It proves nothing new about any other subsystem. `bedrock::net::RequestHeaderPolicy` (PR #52) is
**absent** from the binary: nothing in Chromium's network stack calls it yet, so `--gc-sections`
drops it. Referrer and Client-Hints control is host-tested, not enforced. The enforced list below
is unchanged at one feature.

The lasting finding is about the *test gap*, not about the code: a 12-hour build was spent to learn
one thing `g++` could never tell us. Every constraint of that kind now has a gate that runs in
seconds (`scripts/check_toolchain_limits.py`, invariant 82).

## Build 4 — 2026-08-27 (a claim tested and withdrawn)

| | |
| --- | --- |
| Base | the build 3 output directory, incrementally rebuilt — same revision, same GN args |
| Change | `patches/bedrock/integration/0002-bedrock-local-state-defaults.patch`: typed and scoped pref assignments, a Local State registration loop, and an effective-value line for the reporting-consent pref |
| Objects rebuilt | `browser_prefs.o`, `chrome_metrics_services_manager_client.o`, `browser_ui_prefs.o`, `bedrock/startup.o` |
| Link | `chrome` relinked, exit 0 (2 m 19 s – 3 m 14 s per iteration) |
| Binary | `out/Release/chrome`, 194 189 768 bytes |
| Symbols | `nm -C out/Release/chrome \| grep bedrock::` → **23** (build 3: 18) |
| Run | headless with `--remote-debugging-port`, log `/work/build-logs/run4c.log` |

The intent was to enforce two more shipped defaults: `telemetry` and `crash_reporting`, which in
Chromium are one consent bit — `user_experience_metrics.reporting_enabled`, read by
`ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled` for both metrics and Crashpad
upload. Bedrock now registers that pref's default from its own defaults table.

**The claim did not survive its own test.** The build was run twice: once with Bedrock asking for
`false`, once with Bedrock asking for `true`. Both times the running browser reported:

```
[bedrock] effective user_experience_metrics.reporting_enabled = false (want true: MISMATCH)
```

`MetricsServiceAccessor::IsMetricsReportingEnabled` returns false unconditionally unless
`GOOGLE_CHROME_BRANDING` is set. This build is quiet because it is unbranded, not because of
Bedrock — so the two defaults stay in the **unenforced** list, with that measurement as the reason,
and the startup log says `registering (not decisive in this build)` rather than `enforcing`:

```
[bedrock] Balanced Privacy: 1 of 12 shipped defaults enforced by this build
[bedrock] registering (not decisive in this build) telemetry: user_experience_metrics.reporting_enabled = false
[bedrock] registering (not decisive in this build) crash_reporting: user_experience_metrics.reporting_enabled = false
[bedrock] enforcing webrtc_privacy: webrtc.ip_handling_policy = default_public_interface_only
```

### What this build proves

The integration seam now carries boolean prefs and Local State prefs, not only profile strings, and
a registration loop that skips a pref upstream no longer registers instead of crashing. The
enforced count is unchanged at **one** feature — and the mechanism that keeps that number honest is
now itself tested: an assignment whose `decides_behavior` is false must also appear in the
unenforced list (invariant 83).

The lasting lesson: "the browser does X and our pref says X" is not evidence. Flip the pref to the
wrong value and rebuild — if the behaviour does not follow, the pref was never the cause.

## Enforced features

| Feature | Enforced since | Where the browser performs it | Evidence |
| --- | --- | --- | --- |
| `webrtc_policy` (`webrtc_privacy` default) | Build 2, 2026-08-23 (re-verified builds 3 and 4) | `chrome/browser/ui/browser_ui_prefs.cc` registers the pref with the Bedrock default; `chrome/browser/renderer_preferences_util.cc` passes it to every renderer | `[bedrock] effective webrtc.ip_handling_policy = default_public_interface_only (want default_public_interface_only: match)` in `/work/build-logs/run2.log`; reproduce with `scripts/resume_build.sh` |

## Constraints this build discovered

Recorded here because they are properties of building *inside* Chromium, and they will bite again:

1. **`-fno-exceptions`.** `std::stoi` / `std::stod` are unusable; parsing failures must be return
   values. Two call sites were rewritten on `strtol`/`strtod`.
2. **The `chromium-rawptr` clang plugin.** Pointer fields want `raw_ptr<T>`, which lives in
   `//base`. The overlay deliberately does not depend on `//base` (its host tests build with plain
   `g++` and no Chromium checkout), so `//bedrock` removes the `find_bad_constructs` config and
   documents each of the eight non-owning back-pointers it keeps. A component that starts holding a
   real browser object moves to its own target that does depend on `//base`.
3. **Chromium compiles with C++20 modules for the standard library.** Every standard symbol needs
   its own header included in the file that uses it; libstdc++'s transitive includes hide the
   omission from `g++`. 103 overlay files were missing at least one include and now list them.
4. **Objects can silently go stale** when the build is driven by hand around a siso stall. A
   `startup.o` one day older than its source failed the link with `undefined symbol:
   bedrock::integration::PrefDefaultOr`, and a stale `chrome_browser_main.o` kept executing a hook
   that had already been reverted in the tree. Recompile the object of every file you touch before
   linking; `build/LOCAL_BUILD_HANDOFF.md` §4.9 has the recipe.
5. **`is_component_build=true` binaries need `LD_LIBRARY_PATH=out/Release`** to start at all.
