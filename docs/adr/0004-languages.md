# ADR 0004 — Languages: C++ for the engine, Rust behind an FFI boundary, TypeScript for UI, never Electron

**Status:** accepted (2026-08-21) · roadmap item 48

## Context

Item 48 assigns languages by purpose: C++ for engine integration, Chromium internals,
performance-critical and security-sensitive native code; Rust for new memory-safe modules,
parsers, isolated components; TypeScript/HTML/CSS for UI, settings and management surfaces; and
no Electron for the browser shell.

Chromium constrains all three. It builds with GN/Ninja and its own toolchain; Rust is supported
but only through `//build/rust` with vetted crates and a C-ABI or `cxx`-style boundary; WebUI is
TypeScript compiled by the tree's own toolchain, not by a bundler we bring.

## Decision

| Language | Used for | Not used for |
| --- | --- | --- |
| **C++17** | everything that touches Chromium types, the render/network path, and all policy that must run in the browser process; every subsystem in `src_overrides/bedrock/` today | new standalone parsers of untrusted input, where Rust is the better default |
| **Rust** | new memory-safe modules that can sit behind a narrow interface: parsers of untrusted input, isolated transforms, self-contained privacy processing | anything needing broad Chromium API access — the FFI surface would exceed the safety it buys |
| **TypeScript / HTML / CSS** | WebUI surfaces: settings, Privacy Center, knowledge center, per-site panel, DevTools panels | browser process logic, blocking decisions, or anything a page's timing can observe |
| **Electron / bundled Node** | nothing, ever | the shell, any window, any tool that ships to users |

### Rules

1. **The shell is Chromium's.** No Electron, no Node runtime in a shipped artifact, no
   `node_modules` at runtime. Node exists only as a build-time toolchain, as in Chromium itself.
2. **Rust enters through one door.** A Rust module ships as its own crate with: a C-ABI
   boundary in one `ffi.rs`, `#![forbid(unsafe_code)]` everywhere except that file, no panics
   across the boundary (`catch_unwind` at the edge), `#![no_std]`-friendly where practical, a
   provenance row plus notice file for every crate it pulls in, and its own tests. A Rust
   module without a documented boundary is rejected — Rust "sprinkled" through the browser
   process buys unsafe glue, not safety.
3. **The default for a new untrusted-input parser is Rust; the exception is written down.**
   Existing C++ parsers (filter lists, omnibox input, download names, bookmark import) stay in
   C++ and stay fuzzed (item 43); rewriting working, fuzzed code for language purity is not a
   safety improvement. The filter-list parser is the first migration candidate if a campaign
   finds a memory bug there.
4. **TypeScript stays in WebUI.** No privacy decision is made in TS: the UI renders state from
   `privacy/core` and calls into it. A rule enforced in the UI is a rule that is off when the
   UI is not open.
5. **No language runtime is added to the product.** No Python, no Node, no JVM at runtime.

### Status today

No Rust module has landed. Every subsystem is C++17, dependency-free and host-testable, and the
WebUI surfaces are still specified in design docs rather than implemented. That is stated here
so nobody reads this ADR as a description of code that exists.

## Consequences

- `scripts/check_languages.py` enforces the mechanically checkable parts: no Electron or
  runtime Node dependency, no `.ts`/`.html`/`.css` outside the WebUI directories, Rust crates
  only with an `ffi.rs` and a provenance row, and no unexpected language appearing in the tree.
- Adding Rust later costs a toolchain in CI. Until a crate exists, CI stays free of it, and the
  gate says so rather than pretending a Rust build runs.
