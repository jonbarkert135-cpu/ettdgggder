# 040 — Privacy Knowledge Center

**PrivacyTools.io brief items 6–11, 14–19.** Status: landed and host-tested
(`src_overrides/bedrock/settings/knowledge/knowledge_base.{h,cc}`,
`src_overrides/bedrock/settings/privacy_posture.{h,cc}`).

22 categories, from Privacy Basics to Travel Privacy, each with a stable name.

## Two layers, because of licensing (items 7, 8, 9, 15)

- **Layer A — native Bedrock guides.** Written here, stored locally, readable offline. *How
  Browser Fingerprinting Works*, *How Third-Party Tracking Works*, *How Cookies Track Users*,
  *What Tor Does and Does Not Protect*, *How Extensions Affect Privacy*, *Configuring Bedrock for
  Strong Privacy*. They may cite anyone.
- **Layer B — external reference cards.** Title, source badge, link out. No copied text.

`KnowledgeBase::Add()` refuses:

- a Layer A entry whose provenance says redistribution is **not** allowed — storing the text *is*
  republication;
- a Layer B card that carries a body (a copy smuggled into a "summary");
- any entry that requires attribution but has no attribution text.

That one method is what keeps the knowledge base from quietly becoming a scraper.

Every entry carries a full provenance record: source, URL, author, license, published date,
updated date, redistribution allowed, attribution required, contains third-party material. A
curator's license never covers the tools it links to (`docs/LICENSING.md` §8).

## Source badge (item 9)

External material renders as *Source · PrivacyTools.io · Original article · ↗ Open source website*;
Bedrock material renders as a Bedrock guide with no external link. The user can always see where
Bedrock ends and someone else's site begins.

## Search (item 10) and offline-first (item 11)

Local index over title, summary, category, tags, keywords, source and threat level. No query
leaves the machine — there is no remote search path to disable. `SearchOffline()` ranks locally
available material above cards that need a network, because a result you cannot open on a plane is
a worse answer.

## Knowledge wired to settings (items 16, 17)

`LinkSetting("privacy.cookies.third_party", "cookies")` binds a setting to the article that
explains it, so *Third-party cookies · Blocked · Why? · Learn why →* opens a **local** article.
A setting with nothing honest to link to links to nothing rather than to a generic page.

## Privacy configuration, not a score (item 19)

`PrivacyPosture` reports mechanism states — Tracking protection, Fingerprint protection, Cookie
isolation, Network privacy, Extensions (a count, not a verdict). Network privacy is the **weakest**
of its parts, not their average.

There is no percentage. "You are 97% anonymous" has no denominator, no specified adversary, and
moves whenever we change our own formula. The test scans every rendered value for `%`, `score`,
`anonymous` and friends: the temptation to ship a gamified number is permanent, so the prohibition
lives in code rather than in this paragraph.
