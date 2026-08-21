# 036 — DevTools

**Roadmap item 36.** Status: landed and host-tested
(`src_overrides/bedrock/devtools/privacy_devtools.{h,cc}`).

## Rule zero: do not break DevTools

Every upstream panel, protocol domain and shortcut keeps working exactly as in Chromium. Bedrock
**adds**; it removes and rewrites nothing. Web developers debug in this browser or they leave, and
a privacy browser nobody can develop against has a very small future.

Enforced in code:

- `ModifiesUpstreamProtocol()` is `false` — Bedrock data arrives over its own `Bedrock.privacy`
  domain, so a stock CDP client still works against this browser.
- `DegradesGracefully()` is `true` — if the Bedrock front-end fails to load, DevTools opens
  without the privacy panels rather than not at all.
- `UpstreamPanelsPreserved()` lists the panels that must keep working (elements, console, sources,
  network, performance, memory, application, security, lighthouse, recorder); the test asserts the
  list is upstream panels and not ours.
- `InspectShortcut()` is still F12. Bedrock adds one extra entry point, `Ctrl+Shift+Y`, straight to
  the privacy panels.

## The seven privacy panels

Blocked requests · Trackers · Fingerprint protection · Storage partition · Cookie state ·
Permissions · Connection security. Panel ids are namespaced `bedrock-*` and unique.

Every panel is a **view over the privacy event log** (item 37), not its own counter. The blocked
requests table shows, per row, the third party, the **pipeline stage that decided** (item 13) and
the **exact rule that matched** — a blocker that cannot explain a decision is a blocker developers
learn to distrust. Allowed requests and HTTPS upgrades are not listed as "blocked", because they
were not.

Panels for an unmeasured page report *not measured* rather than 0.
