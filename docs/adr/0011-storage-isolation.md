# ADR 0011 — Storage is partitioned by top-level site, and third-party storage is ephemeral

**Status:** accepted (2026-08-21) · roadmap items 15, 20 · owner's list: ADR-007
**Design note:** `docs/design/015-storage-isolation.md`

## Context

Third-party cookies are the identifier everyone talks about. They are also the one that is
already dying, which is why tracking moved years ago into localStorage, IndexedDB, service worker
registrations, the HTTP cache, and every other place a page can leave a byte behind. Blocking
cookies alone moves the problem rather than solving it.

The counter-pressure is real: a site embedded on two pages that suddenly cannot share state
breaks single sign-on, embedded checkout and comment widgets.

## Decision

**Every storage backend is keyed by (origin, top-level site), and a third party that is not
otherwise allowed gets storage that does not survive the tab.**

* Partitioning covers cookies, localStorage, sessionStorage, IndexedDB, CacheStorage, service
  workers, the HTTP cache and the DNS/connection pools where the same key applies. A partial
  partition is a partition with a hole in it, and the hole is where tracking goes.
* Third-party storage that is not user-approved is **ephemeral**: it lives in memory for the
  lifetime of the tab and is discarded on close. The embedded widget works during a visit and
  does not recognise the user on the next one.
* The **Storage Access API** is the sanctioned escape: a third party may ask, the user may grant,
  and the grant is per top-level site, revocable, and visible in the per-site panel.
* Private windows and Tor mode get their own partition space that is destroyed with the session.

## Alternatives considered

* **Block third-party storage outright.** Cleanest, and it breaks federated login for a large
  share of the web. Available as the Strict privacy choice, not as the default (item 84 ships
  third-party cookies *restricted*, and `docs/DEFAULTS.md` says why).
* **Cookie blocking only.** Rejected as described above: it is the 2015 answer.

## Consequences

* Cache partitioning costs measurable network traffic — the same font fetched on two sites is two
  fetches. That cost is recorded as `performance_cost` in the trade-off table rather than hidden.
* Ephemeral third-party storage needs a defined lifetime for edge cases (a tab restored from
  session history, a bfcache entry). That lifetime is an open item in `.ai/memory/STATE.md`.
