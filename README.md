<p align="center">
  <img src="branding/bedrock-logo.png" width="140" alt="Bedrock Browser">
</p>

<h1 align="center">Bedrock Browser</h1>

<p align="center">
  A production-grade, fully autonomous, open-source desktop browser built on Chromium.<br>
  No vendor backend. No account. No telemetry. Licensing recorded before code.
</p>

---

## What this is

Bedrock is a **Chromium-derived browser**, not an Electron app, not a WebView wrapper, not a
shell over someone else's build. This repository is the **overlay**: patches, new source files,
build args, branding and tooling. `build/sync.py` fetches the pinned Chromium tree and applies
them. See [ADR 0001](docs/adr/0001-chromium-overlay.md) for why overlay and not fork.

## Principles

1. **Autonomous.** Everything — history, bookmarks, passwords, profiles, sessions, settings,
   extensions, downloads, privacy engine, content blocker, fingerprint protection, cookies,
   permissions, search, themes — works on the user's device. Bedrock operates **no server** of
   any kind: no cloud backend, account, sync, telemetry, analytics, proxy or VPN.
   The browser still talks to the sites, search engine and DNS resolver the *user* chooses —
   it simply never inserts our infrastructure in between.
2. **Licensing first.** Nothing lands without a provenance row and a notice file.
   CI enforces it: [`scripts/check_provenance.py`](scripts/check_provenance.py).
3. **Inspiration, not copying.** Brave, Firefox, Tor Browser, uBlock Origin and Privacy Badger
   inform the design. What is legally reusable, what is reimplemented and what is off-limits is
   decided per project in [`docs/THIRD_PARTY.md`](docs/THIRD_PARTY.md) — including the hard
   GPL-3.0 boundary around uBlock Origin and Privacy Badger.

## Repository layout

```
build/          chromium.pin (pinned base), sync.py (fetch + overlay), args/*.gn
patches/        patches against the Chromium tree (bedrock/ and upstream/<project>/)
src_overrides/  new files mirrored into the Chromium tree layout (preferred over patches)
docs/           LICENSING.md, THIRD_PARTY.md, BUILD.md, adr/
THIRD_PARTY_NOTICES/  one notice file per dependency, 1:1 with the inventory
scripts/        check_provenance.py — the licensing gate
branding/       Bedrock name and logo assets
```

## Build

See [docs/BUILD.md](docs/BUILD.md). Short version, Linux x64:

```bash
python3 build/sync.py --workspace ~/bedrock-src   # ~100 GB, long
# then the gn gen / autoninja commands sync.py prints
```

## Status

| Roadmap | State |
|---|---|
| 1–5 Foundation (engine, autonomy, licensing) | done — overlay build system + provenance gate |
| 6 Search engine system | designed + selection logic landed, host-tested |
| 7 Address bar / omnibox | designed + input classifier landed, host-tested |
| 8 Privacy Engine | architecture + feature registry landed |
| 9–10 Anti-fingerprinting | 4 levels, deterministic derivation, 21 documented surfaces |
| 11 Protection Controller | per-site/domain/global resolver landed, host-tested |
| 12 Content blocker | ABP/uBO-syntax filter engine, token-indexed, 50k rules in ~0.2 us |
| 13 One blocking pipeline | single `Evaluate()`; lists, heuristic and shields are stages |
| 14 Behavioral detection | Privacy Badger-style local learning, GPC/DNT, link cleaning |
| 15 Storage isolation | one StorageKey for every backend, incl. cache, DNS and HSTS |
| 16 HTTPS | upgrade / HTTPS-Only, mixed content, per-host cert exceptions only |
| 17 DNS | system default, named DoH providers, fail-closed strict mode |
| 18 WebRTC | Default / Privacy / Strict, no local IP outside Default |
| Extension catalog | designed ([009](docs/design/009-extension-catalog.md)) |

Design docs live in [`docs/design/`](docs/design). Pure logic ships with dependency-free host
tests — `./scripts/run_host_tests.sh` builds and runs them with plain `g++`, no Chromium
checkout required, and CI runs them on every PR.

## License

Bedrock's own code: **MPL-2.0** ([LICENSE](LICENSE)) — chosen for compatibility with Chromium's
BSD-3 base and with MPL-2.0 sources such as brave-core. Rationale in
[docs/LICENSING.md](docs/LICENSING.md).

Bedrock is not affiliated with, endorsed by or sponsored by Google, Brave Software, Mozilla,
the Tor Project, Raymond Hill or the Electronic Frontier Foundation. All trademarks belong to
their owners and are used descriptively only.
