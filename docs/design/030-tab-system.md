# 030 — Tab system

**Roadmap item 30.** Status: landed and host-tested
(`src_overrides/bedrock/ui/tab_model.{h,cc}`).

**One model, two layouts.** Horizontal and vertical tabs render the same ordered list. A browser
that keeps two tab models eventually shows different tabs in the two layouts, and the bug is
unreproducible for whoever does not use the other one.

**Ordering is the model's job**: pinned first, then grouped tabs with each group contiguous, then
the rest — maintained by a stable sort after every structural change, so the UI never sorts and
therefore cannot sort differently. Unpinning drops the tab at the start of the unpinned section,
where the user is looking, instead of teleporting it back to where it used to live.

| Feature | Notes |
|---|---|
| Groups | named, contiguous, closing a group leaves every tab in recently-closed |
| Pinned | first in order, never slept |
| Muted / audible | tracked separately: muting is a user decision, audibility is a fact |
| Sleeping | idle background tabs are discarded — **never** the active tab, a pinned tab, or one making sound. Those three exceptions are what make the feature tolerable |
| Tab search | case-insensitive, over title and URL, in tab order |
| Duplicate detection | `NormalizeUrl()` ignores scheme, `www.`, trailing slash, fragment and tracking parameters — real query parameters are kept, because `?id=7` is a different page |
| Recently closed | capped at 25, reopened at the index it had |

Closing the active tab activates its neighbour, not nothing — an empty selection is a state every
call site downstream would have to handle, and one of them would forget.
