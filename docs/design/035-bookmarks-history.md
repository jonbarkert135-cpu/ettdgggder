# 035 — Bookmarks and history

**Roadmap item 35.** Status: landed and host-tested
(`src_overrides/bedrock/data/bookmark_store.{h,cc}`, `data/history_store.{h,cc}`).

## Bookmarks

Folders **and** tags, because they answer different questions: a folder is where a bookmark
lives, a tag is what it is about. Search covers title, URL and tags in one query; a leading `#`
limits it to tags. A bookmark the user cannot find again is a bookmark they did not make.

- **Removing a folder moves its contents up to the parent.** "Delete folder" losing fifty
  bookmarks is not a feature.
- Tags are stored lower-case, so `Privacy` and `privacy` are one tag.
- **Import/export is the Netscape bookmark file**, the format every browser reads and writes —
  tags travel in the `TAGS` attribute, as Firefox writes them. An export nobody else can open is
  lock-in, and this project has no business locking anyone in. Escaping round-trips (`&`, quotes,
  angle brackets are asserted).
- **Import adds, never replaces.** The test keeps a pre-existing bookmark across an import.

## History

Search, domain grouping, date grouping, and deletion by entry / URL / domain / date range / all.

The interesting requirement is **deletion**, not search. In most browsers, deleting a history
entry removes the visible row and leaves the derived data behind — visit counts, omnibox ranking,
typed-URL scores, top sites — so the deleted page keeps suggesting itself in the address bar.
That is worse than offering no deletion at all, because the user believes it is gone.

Every delete path here goes through `Forget()`, which drops the visits *and* the derived signals,
and the test asserts `RankingScore()` returns 0 afterwards. One nuance: deleting one of five
visits to a URL does not wipe the ranking for the other four — the signal goes when the last
visit does.

Typed visits weigh 3, followed links 1 — the derived-data problem in one line, which is exactly
why `Forget()` exists. `DeleteDomain()` accepts a full URL and matches on the host (scheme, `www.`,
port and path stripped), so "delete everything from this site" works from a right-click on a row.
