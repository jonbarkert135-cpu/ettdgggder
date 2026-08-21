# 020 — Private window

**Roadmap item 20.** Status: landed and host-tested
(`session/browsing_mode.{h,cc}`, `session/new_identity.{h,cc}`).

A private window is `IsolationLevel::kEphemeralAll` plus no history recording. Closing it runs
the **same machinery as New Identity** (item 22) with one plan difference, so there is one
teardown implementation and one place where a forgotten data type would show up.

Cleared on close: cookies, site storage (localStorage, IndexedDB, CacheStorage, FileSystem),
HTTP cache, Service Workers, temporary permissions, form data, session history and tabs, network
state (sockets, DNS cache, TLS sessions), media device salts, fingerprint seed.

Never touched, in either plan: **bookmarks, settings, saved passwords, installed extensions, and
files already downloaded to disk.** The normal window's history and another session's Tor
circuits are also left alone — they belong to a different session, and clearing them would be a
surprise, not a feature.

What a private window does *not* do is stated in the window itself: the network, the employer
and the sites visited still see the traffic. "Private" here means "not stored on this device",
and the UI says so rather than letting the user infer something larger.
