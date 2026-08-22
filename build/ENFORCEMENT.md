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

## Enforced features

| Feature | Enforced since | Where the browser performs it | Evidence |
| --- | --- | --- | --- |
| _(none yet)_ | | | |

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
