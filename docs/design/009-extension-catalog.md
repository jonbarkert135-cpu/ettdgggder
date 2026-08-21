# 009 — Extension catalog (the "store")

Not a numbered roadmap item — requested in chat on 2026-08-21: *«сделаем магазин расширений
всех топовых + по умолчанию поставь то что от Brave»*.

## Decision

1. **Default blocking is built in**, not an extension: the adblock-rust Privacy Engine
   (item 8). A fresh profile blocks ads and trackers with **zero** extensions installed.
2. **`bedrock://extensions/catalog`** is a curated list of well-known extensions. It is a
   *catalog*, not a store: no accounts, no payments, no Bedrock-hosted review system, no
   Bedrock server. The catalog itself is a JSON file shipped with the browser and refreshed
   from the project's public repository — which is data, not infrastructure.
3. Each entry installs **from the extension author's own official source** (their GitHub
   release CRX/XPI, their site). Bedrock does not rehost binaries, so we never become the
   distributor of someone else's code and never become a supply-chain single point.

## Why not the Chrome Web Store

Its API requires Google keys we deliberately do not ship, its Terms restrict access to
Chrome-branded builds, and routing every user through Google contradicts item 4/5. Users can
still install any CRX manually and any extension the catalog links to.

## Legal shape (this is the part that matters)

| Path | Allowed? | Condition |
|---|---|---|
| Link to uBlock Origin / Privacy Badger, user installs | **Yes** | Aggregation. No GPL obligation on Bedrock. |
| Ship uBO/PB CRX inside the Bedrock installer | Possible, not done | GPL-3.0 permits redistributing the unmodified work, but then Bedrock's installer must carry its full license text and a written source offer. Extra obligation for no benefit — the catalog link achieves the same. |
| Link the code into the browser binary | **Never** | Relicenses the binary. See `docs/LICENSING.md` §3. |
| Pre-enable an extension without consent | No | The user chooses what runs. |

Extension names and logos in the catalog are used descriptively; each entry shows the
extension's own license and links its source.

## Initial catalog

Content blocking (all optional, the built-in engine covers this): uBlock Origin, Privacy Badger.
Other categories: Bitwarden / KeePassXC-Browser (passwords), Dark Reader, SponsorBlock,
LocalCDN, ClearURLs, Vimium, Tampermonkey, Wayback Machine.

Every entry carries: id, name, author, license, official source URL, permissions summary, a
one-line "what it does", and a note when it duplicates a built-in feature (ClearURLs and uBO
both overlap the Privacy Engine — say so instead of letting users double-block and then
debug a broken site).

## Manifest V3

Bedrock keeps MV2-era blocking capability (`webRequestBlocking`) available for as long as
Chromium's code allows, and the built-in engine is not affected by MV3 rule limits at all —
that is the main reason blocking is built in rather than left to extensions.
