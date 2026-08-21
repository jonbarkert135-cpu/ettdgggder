# Storage isolation

**Surface id:** `storage-isolation` · **Levels:** 0–2 normalize (partitioned) · 3 block (ephemeral)

## Attack vector
Fingerprinting is only worth doing if the result can be stored and re-read. Cookies, localStorage, IndexedDB, cache, Service Workers, CacheStorage, HSTS entries and favicon caches have all been used as cross-site supercookies.

## Mitigation
All third-party storage — including HTTP cache, HSTS state and Service Worker registrations — is partitioned by top-level site at every level, including level 0: this is a structural guarantee, not a fingerprinting shim, so it is never traded away for compatibility. Level 3 additionally makes third-party storage ephemeral (cleared when the top-level site is closed).

## Compatibility impact
Federated logins that rely on third-party cookies use the Storage Access API prompt. Level 3 signs the user out of embedded services when the tab closes; flagged.

## Performance impact
Slightly lower cache hit rate across sites — the accepted cost of partitioning.

## Test cases
- A third-party iframe on site A cannot read the value it wrote on site B.
- Cache timing cannot detect a resource fetched on another site.
- Service Worker registered in a third-party frame is scoped to the partition.
