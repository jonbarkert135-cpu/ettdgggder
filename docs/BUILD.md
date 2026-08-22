# Building Bedrock

**Roadmap item 73.** Every step, with versions, flags and packaging, for Linux and Windows.
No step says "install the dependencies and build".

Two honest warnings before the first command:

- **The Linux path has now been executed end to end once** (2026-08-22, pinned revision, overlay
  compiled in-tree — see [`../build/ENFORCEMENT.md`](../build/ENFORCEMENT.md) for the numbers). The
  Windows path has not. Neither runs in CI. Where a value must match the Chromium tree exactly,
  this document gives the command that prints the authoritative value instead of a number that
  will be stale in a month.
- **A first build is measured in hours and hundreds of gigabytes.** The reference build took
  **12 h 16 m** for 56 105 steps on 17 cores, and produced a 194 MB binary. Nothing about that is
  Bedrock's doing; it is what building a browser engine costs.

## Requirements

| | Linux (reference) | Windows |
| --- | --- | --- |
| Disk | 100 GB free (checkout ~40 GB, one `out/` ~40 GB) | 130 GB — NTFS, and it is slower |
| RAM | 16 GB minimum, 32 GB comfortable | same |
| CPU | 8 cores; first build 3–8 h, incremental minutes | same, plus Defender exclusions |
| OS | Ubuntu 22.04+ / Debian 12+ x64 (others work, `install-build-deps.sh` targets these) | Windows 10 22H2 or Windows 11, x64 |
| Python | 3.9+ (`python3`) | 3.9+, on `PATH` as `python3` |
| Git | 2.34+ | 2.34+, with long paths enabled |

Chromium pin: see [`build/chromium.pin`](../build/chromium.pin). The build refuses to proceed
against a different revision than the one recorded there.

## Common step: fetch Chromium and apply the overlay

```bash
git clone https://github.com/jonbarkert135-cpu/ettdgggder bedrock
cd bedrock
python3 build/sync.py --workspace ~/bedrock-src
```

`sync.py` clones depot_tools, fetches the pinned Chromium revision, runs `gclient sync`, applies
`patches/` and copies `src_overrides/` into the tree. It prints the exact `gn`/`ninja` commands with
paths filled in when it finishes. After changing overlay files, re-run only the overlay step:

```bash
python3 build/sync.py --workspace ~/bedrock-src --overlay-only
```

## Linux

### 1. Toolchain and dependencies

```bash
# Chromium's own dependency installer — the authoritative list for the pinned revision.
~/bedrock-src/src/build/install-build-deps.sh --no-prompt

# Compiler and Rust toolchain come from the Chromium checkout, not the distribution:
#   third_party/llvm-build/Release+Asserts/bin/clang++
#   third_party/rust-toolchain/bin/rustc
# gclient sync fetched both. Do not substitute the system clang — a different
# compiler produces a different binary and breaks reproducibility (REPRODUCIBILITY.md).
ls ~/bedrock-src/src/third_party/llvm-build/Release+Asserts/bin/clang++
```

Wayland and X11 are both supported ([`PLATFORMS.md`](PLATFORMS.md)); `use_ozone = true` in the
release args covers both, no separate build needed.

### 2. Configure

```bash
export PATH=~/bedrock-src/depot_tools:$PATH
cd ~/bedrock-src/src
gn gen out/Bedrock --args="$(grep -v '^#' ~/bedrock/build/args/bedrock-release.gn | tr '\n' ' ')"
gn args out/Bedrock --list --short   # inspect the resolved values
```

The release arguments live in [`build/args/bedrock-release.gn`](../build/args/bedrock-release.gn)
and are CI-checked: `check_no_telemetry.py` fails if `enable_reporting`, `safe_browsing_mode` or the
Google API key flags drift, and `check_provenance.py` fails if the pin and the inventory disagree.
Do not hand-edit the args for a release build — change the file, so the change is reviewable and the
GN args hash in the build manifest still means something.

### 3. Build and run

```bash
autoninja -C out/Bedrock chrome          # the browser
autoninja -C out/Bedrock chrome unit_tests content_shell   # what CI would build
./out/Bedrock/chrome
```

`autoninja` picks the job count; on a memory-constrained machine use
`ninja -C out/Bedrock -j$(nproc --ignore=2) chrome` instead of raising it.

### 4. Package

| Format | Command | Notes |
| --- | --- | --- |
| `.tar.xz` | `tar -C out/Bedrock -cJf bedrock-<version>-linux-x64.tar.xz .` | the reference artifact; what the release manifest signs |
| `.deb` | `autoninja -C out/Bedrock "chrome/installer/linux:unstable_deb"` | Chromium's own installer targets |
| `.rpm` | `autoninja -C out/Bedrock "chrome/installer/linux:unstable_rpm"` | |
| AppImage / Flatpak | out of tree, from the `.tar.xz` | not produced by the project today |
| snap | — | **not produced**, deliberately ([`PLATFORMS.md`](PLATFORMS.md)) |

Desktop integration files (`.desktop`, icons in the hicolor tree) come from
`python3 scripts/gen_icons.py --out out/branding`.

## Windows

### 1. Prerequisites, in this order

1. **Visual Studio 2022** (Community is fine), with these components — the exact set Chromium's
   `vs_toolchain.py` expects:
   - Desktop development with C++
   - MSVC v143 build tools (x64/x86)
   - C++ ATL and C++ MFC for v143
   - Windows 11 SDK — the version Chromium requires for the pinned milestone. Print it rather than
     guessing: `python3 build/vs_toolchain.py get_toolchain_dir` in the fetched tree names the SDK
     it wants, and `gn gen` fails with the required version if it is absent.
   - Debugging Tools for Windows (installed via *Windows SDK → Modify → Debugging Tools*) —
     required, and the component most often missing.
2. **Long paths**, or the Chromium checkout will fail halfway with unhelpful errors:
   ```powershell
   git config --global core.longpaths true
   New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" `
     -Name LongPathsEnabled -Value 1 -PropertyType DWORD -Force
   ```
3. **Environment** (non-Google builds must not look for the internal toolchain package):
   ```powershell
   setx DEPOT_TOOLS_WIN_TOOLCHAIN 0
   setx GYP_MSVS_VERSION 2022
   ```
4. **Defender exclusions** for the checkout and `out/` directories. Without them a build can take
   twice as long; this is a build-time recommendation, not a security recommendation for the
   machine in general.
5. Case-sensitive NTFS is **not** required; a case-insensitive volume is fine.

### 2. Fetch and configure

```powershell
python3 build\sync.py --workspace C:\src\bedrock
$env:PATH = "C:\src\bedrock\depot_tools;$env:PATH"
cd C:\src\bedrock\src
gn gen out\Bedrock --args="$((Get-Content ..\..\bedrock\build\args\bedrock-release.gn |
                              Where-Object {$_ -notmatch '^#'}) -join ' ')"
```

### 3. Build, run, package

```powershell
autoninja -C out\Bedrock chrome
.\out\Bedrock\chrome.exe

# Installer + portable zip (Chromium's own targets)
autoninja -C out\Bedrock mini_installer
```

| Artifact | Produced by | Notes |
| --- | --- | --- |
| `mini_installer.exe` | `autoninja -C out\Bedrock mini_installer` | per-user install by default |
| portable `.zip` | zip the `out\Bedrock` payload subset listed by `chrome/tools/build/win/FILES.cfg` | |
| MSI | not produced | enterprise packaging is a future task, tracked honestly rather than half-implemented |

Code signing: the artifact is signed as part of release, not of build
([`SUPPLY_CHAIN.md`](SUPPLY_CHAIN.md)). Do not sign a local build with a release key.

## What Chromium's toolchain requires from overlay code

The overlay is plain C++17 and its host tests build with `g++`, but inside the Chromium tree it is
compiled by Chromium's clang with Chromium's flags and plugins. Three differences are strict, and
all three were found the hard way by the first in-tree build:

| Constraint | What breaks | What to do instead |
| --- | --- | --- |
| `-fno-exceptions` | `std::stoi`, `std::stod`, any `try`/`catch` | parse with `strtol`/`strtod` and return a failure value |
| `chromium-rawptr` clang plugin | raw pointer *fields* in classes | `//bedrock` removes `find_bad_constructs` and documents its non-owning back-pointers; a target that depends on `//base` must use `raw_ptr<T>` |
| C++20 standard-library modules | a standard symbol used without including its own header — libstdc++ hides this, clang does not | include `<utility>`, `<cstdint>`, `<cstddef>`, … in every file that names a symbol from it |

Check the first two before pushing, without a Chromium checkout:

```bash
grep -rn 'std::sto[id]\|catch (' src_overrides/   # must find nothing outside comments
```

`scripts/gen_build_gn.py` owns `src_overrides/bedrock/BUILD.gn`; add a source file and re-run it
with `--write`. The gate in `scripts/run_host_tests.sh` fails if the generated file is stale, so a
new file cannot silently miss the engine build.

## Debug and sanitizer builds

```bash
gn gen out/Debug  --args='is_debug=true is_component_build=true'
gn gen out/ASan   --args='is_asan=true is_debug=false enable_full_stack_frames_for_profiling=true'
```

Sanitizer and fuzzer configurations, and what CI would run with them, are in
[`security/TESTING.md`](security/TESTING.md).

## Reproducible release builds

A release build differs from a local one in exactly three ways, all of them required by
[`REPRODUCIBILITY.md`](REPRODUCIBILITY.md):

```bash
export SOURCE_DATE_EPOCH=$(git -C ~/bedrock log -1 --format=%ct)   # overlay commit date
export LANG=C.UTF-8 TZ=UTC
gn gen out/Release --args="$(grep -v '^#' ~/bedrock/build/args/bedrock-release.gn | tr '\n' ' ') \
  is_official_build=true chrome_pgo_phase=0"
```

Then record the nine manifest values (overlay commit, Chromium version and commit, GN args hash,
toolchain revision, SBOM hash, `SOURCE_DATE_EPOCH`, builder platform, artifact digests). Two builds
are equal when those match; a mismatch with everything else equal is a bug worth reporting.

## When it fails

| Symptom | Cause |
| --- | --- |
| `gclient sync` fails on a hook | usually a missing distro package — re-run `install-build-deps.sh` |
| `gn gen` complains about a missing SDK | the SDK component or Debugging Tools were not installed (Windows step 1) |
| a patch under `patches/` fails to apply | the pin moved without the patch being re-verified — `python3 scripts/upstream_sync.py --check-patches` |
| the build succeeds but the binary is not Bedrock-branded | the overlay was not applied: re-run `sync.py --overlay-only` |
| out of memory during link | link with fewer jobs: `ninja -C out/Bedrock -j2 chrome` after a full compile |

Ask in an issue with the failing command, the OS version and the output of
`python3 scripts/upstream_sync.py --status`.
