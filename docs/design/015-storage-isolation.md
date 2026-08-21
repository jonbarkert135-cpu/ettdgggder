# 015 — Cookie and storage isolation

**Roadmap item 15.** Status: key model landed and host-tested
(`src_overrides/bedrock/net/storage_isolation.{h,cc}`).

## One key, every backend

```
StorageKey = (origin, top-level site, is-cross-site)
```

Deliberately the same shape as Chromium's `blink::StorageKey` /
`net::NetworkIsolationKey`. Agreeing with the engine is the whole trick: partitioning holds
because every backend derives its key from one function, not because we remembered to patch each
one. A backend that computes its own key is the bug this file exists to prevent.

Partitioned, with no exceptions: cookies, localStorage, sessionStorage, IndexedDB, CacheStorage,
**HTTP cache**, Service Workers, SharedWorkers, blob storage, FileSystem, WebSQL, **DNS cache and
socket pools**, **HSTS state**. The last three are the ones people forget, and each has been used
as a cross-site identifier in published research — a cached resource can be timed, an HSTS entry
is one persistent bit per domain, a warm connection reveals that another site was visited.

`IsPartitioned()` returns true for every type, and a test walks the enum to assert it. There is
no isolation level that turns partitioning off: unpartitioned third-party storage *is* the
tracking mechanism, so the floor is "partitioned and persistent", not "off".

## Levels

| Level | Third-party storage | First-party storage |
|---|---|---|
| Standard (default) | partitioned, persistent | persistent |
| Strict | partitioned, ephemeral | persistent — logins survive |
| Ephemeral all | ephemeral | ephemeral (private windows, FP level 3) |

Session storage is per tab at every level, by definition.

## Storage Access API grants are scoped

When the user allows an embedded service (a federated login) to use its own storage, the grant
applies to **that origin under that one top-level site**. The key stops being cross-site there
and nowhere else, so the exception cannot turn back into a cross-site identifier. Revoking
restores partitioning immediately. Both directions are tested.

## Deletion means deletion

"Clear data for example.com" clears every key whose **top-level site** is example.com —
including what third parties stored while embedded there. Deleting only the site's own origin
would leave the tracking state behind and call it deleted.

## Explanations

`Explain()` returns a plain-language sentence per storage type, and a test requires one for every
enum value. Same rule as the rest of the Privacy Engine: a mechanism nobody can explain cannot
ship.
