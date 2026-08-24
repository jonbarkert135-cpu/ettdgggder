# 049 — CNAME uncloaking

Top of the research queue in `.ai/memory/STATE.md`, and the mechanism
[`docs/research/BRAVE.md`](../research/BRAVE.md) called "the single most valuable
blocking mechanism we do not have".

## The hole it closes

A tracker sells the site a subdomain. `metrics.shop.test` is published as a
CNAME to `collect.tracker.test`, and the tracker's script is served from the
site's own name. To every layer we have, that request is first party:

- the filter lists match on names, and nobody lists `metrics.shop.test`;
- the party check (`third_party()`) compares eTLD+1 and sees a match;
- the cookie policy hands it a first-party cookie;
- the behavioural heuristic never looks at first parties by design.

So the request is not blocked, not partitioned, and not even observed. Resolving
the alias before matching is the only way to see it, which is why this is worth
a DNS-shaped complication.

## Where it sits

One stage in the one pipeline (`MEMORY.md` non-negotiable: exactly one component
decides whether a request is made):

```
request -> shields -> filter lists -> CNAME uncloaking -> party analysis
        -> heuristic -> cookie/script policy -> Allow | Partition | Block
```

After the lists, because a name the lists already catch needs no alias. Before
the party check, because a cloaked tracker is precisely the request the party
check would wave through.

## Four refusals

Each of these is a decision to do less than the obvious thing, and each has a
test in `cname_uncloak_test.cc`.

**1. No DNS lookup on the decision path.** `Canonical()` reads a cache and
nothing else. `Evaluate()` has a 30 µs budget and measures 0.54 µs; a DNS round
trip is four to five orders of magnitude larger. Stalling every request behind a
resolver would be paid by every user on every page, to catch a tracker on the
*first* request only. Instead a miss queues the name, the embedder resolves it
out of band with the user's resolver, and the next request for that host is
matched under the alias. Measured cost of the stage: **0.175 µs** against a
5 µs budget (`cname_uncloak_lookup`).

The honest consequence: **the first request to an unseen cloaked host is not
blocked.** That is written here, in the feature disclosure, and in
`docs/privacy/FEATURES.md` — not buried.

**2. The user's resolver, or nothing.** This component resolves nothing itself.
It hands pending names to the embedder, which must use whatever
`privacy/network/dns_settings` says the user chose. An uncloaking lookup that
reached around a fail-closed DNS setting, or fell back to plaintext DNS to get
its answer, would be a privacy bug committed in the name of privacy.
`SetUnavailable(true)` makes the browser degrade to "no uncloaking" and queue
nothing at all.

**3. Uncloaking can only block, never allow.** The alias is re-matched against
the filter lists and only a *block* is honoured. An `@@` exception on the
canonical name does not become an allow for the request — otherwise a tracker
could CNAME itself into a whitelisted domain and buy an exception.

**4. Only where cloaking is possible.** Already-third-party requests are matched
normally (an alias would change no decision), the site's own apex is never asked
about, and top-level document loads are never uncloaked — retargeting a
navigation is not this component's job.

## Details that matter

| Thing | Choice | Why |
| --- | --- | --- |
| TTL | honoured, clamped to 60 s … 24 h | the *tracker* publishes the TTL: a 1 s TTL would turn this into a lookup per request, a 1 y TTL would outlive the alias |
| Cache | 4096 entries, oldest-expiring evicted | a cache without a ceiling is a memory leak with a hit rate |
| Pending queue | 256 names, deduplicated | page content decides how many names arrive; it must not decide how much memory that costs |
| Alias application | host, eTLD+1 **and** the host inside the URL are replaced together | rewriting only `host` leaves `||collect.tracker.test^` unable to match the URL it exists to match — the quiet way this feature would end up doing nothing |
| Forgetting | `Clear()` for New Identity, `ForgetSite()` both directions | "forget about this site" must drop what the site aliased *and* what aliased to it |
| Same-site alias | reported as `kSameSite`, never blocked | a CDN alias inside your own site is not cloaking |
| Unlisted alias | allowed | being an alias is not evidence of tracking; the lists still decide |

## Status

Host-tested logic, no entry point yet: the DNS client, the resolver plumbing and
the privacy-panel row need the Chromium build (phase 3). No new feature id — this
is part of `tracker_protection`, whose "what it cannot protect" text was updated
in the same change, because that text used to say CNAME trackers are not caught
at all.
