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

## Enforced features

| Feature | Enforced since | Where the browser performs it | Evidence |
| --- | --- | --- | --- |
| `webrtc_policy` (`webrtc_privacy` default) | Build 2, 2026-08-23 | `chrome/browser/ui/browser_ui_prefs.cc` registers the pref with the Bedrock default; `chrome/browser/renderer_preferences_util.cc` passes it to every renderer | `[bedrock] effective webrtc.ip_handling_policy = default_public_interface_only (want default_public_interface_only: match)` in `/work/build-logs/run2.log`; reproduce with `scripts/resume_build.sh` |

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
