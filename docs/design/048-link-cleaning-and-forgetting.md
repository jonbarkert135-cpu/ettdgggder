# 048 — Link cleaning, redirect debouncing, and forgetting a site

Two items from the research queue in `.ai/memory/STATE.md`: *query stripping +
debouncing* and *"forget about this site"*. Both are user-visible actions on a
single site, both are easy to do dishonestly, and both are pure logic — so they
are host-tested here long before a Chromium build exists.

## Link cleaning and debouncing (`privacy/tracker_blocker/url_cleaner`)

Feature `query_param_stripping`, status `policy-landed`.

A click identifier (`?fbclid=…`) is not needed to reach a page; it exists so a
click can be joined to a profile. A redirect wrapper
(`https://out.reddit.com/?url=…`) is a request the user never asked for, made
only so the hop can see the click. One component removes both, and
`BlockingPipeline::CleanUrl()` now delegates to it instead of keeping a second
parameter table (MEMORY.md non-negotiable 4 — the old copy is deleted, not
deprecated).

Order matters: **unwrap first, then strip**. The tracking parameters that matter
are usually inside the wrapped target, so cleaning the wrapper and then
following it would clean the wrong URL.

Deliberate refusals, each with a test:

| Situation | Behaviour | Why |
| --- | --- | --- |
| Subresource (`UrlUse::kSubresource`) | never cleaned | a tracking-shaped parameter on a script or XHR is frequently load-bearing; a half-broken page sends the user to another browser, which is the worse privacy outcome |
| Nothing to strip | returns the input **byte-identical** | a cleaner that normalises URLs it did not need to touch breaks sites for free |
| Fragment | never rewritten | it does not leave the browser on its own, and sites keep application state in it |
| Parameter name matching | exact, case-insensitive, no prefixes or value heuristics | one false positive breaks a checkout |
| Unwrap target that is relative, `javascript:`, `data:`, or the redirector itself | left alone | a cleaner that can retarget a navigation *is* an open-redirect engine |
| Unlisted host | not treated as a redirector | there is a rule per redirector, never a generic "any parameter that looks like a URL" |
| Chain of wrappers | at most `kMaxHops` = 3, remaining wrapper left visible | terminates, and never guesses past what the rules cover |

The parameter table stays short on purpose (23 names, all pure click ids). The
long community lists are attractive and are exactly where the site-breaking
entries live; adding one is a decision with an owner, not a sync job.

## Forget about this site (`privacy/core/forget_site`)

The alternative today is a user clearing *all* cookies to get rid of one site —
which costs them every other login and teaches them not to use the control.

Two rules, both borrowed from New Identity (item 22):

1. **The plan is shown before anything is deleted.** Eight stores, in the order
   "what the site did to you" → "what you wrote": history, cookies and site
   storage, cache (including cached DNS and HSTS entries), site settings,
   behavioural learning, download list rows — then saved passwords and
   bookmarks, **unselected by default** because the user authored those.
2. **Failures are reported, never swallowed.** Each store returns its own
   outcome: `removed`, `nothing stored`, `kept`, `failed`, or `not available in
   this build` when no deleter is registered. `ForgetReport::complete()` is
   false if a single store failed or was unavailable. Deletion the browser did
   not perform is never rendered as done — the entire value of the action is
   that its report can be trusted.

`nothing stored` is deliberately distinct from `removed`: collapsing them makes
it impossible to use the action to find out whether a site stored anything.

Scope is the registrable domain plus subdomains (`cdn.shop.test` is state of
`shop.test`), one profile at a time; profiles share nothing (item 21).

`ForgetSite::Limits()` ships with the plan and says what the action cannot do:
data the site already received, copies on its servers, and other profiles or
devices are out of reach. The test asserts the sentence contains that limit and
none of the banned absolute claims.

Deleters are **injected**, not implemented here: the stores live in Chromium
(cookies, cache, HSTS) or in other Bedrock components (history, heuristic,
downloads). This component owns the plan, the order, the scope and the report.

## Verification

- `url_cleaner_test` — 26 assertions: stripping, byte-identical no-op,
  fragments, case, subresource refusal, four unwrap refusals, hop budget,
  and that `BlockingPipeline::CleanUrl()` is the same code path.
- `forget_site_test` — plan covers every enum value, default selection per
  store, unavailable ≠ removed, unselected stores are never called, failure
  makes the run incomplete, empty site name runs nothing, limits sentence.
- Both run in `scripts/run_host_tests.sh`; no Chromium checkout involved.

## Not done here

Wiring: the omnibox/context-menu entry points for cleaning, the "Copy clean
link" menu item, and the real deleters all need the Chromium build (phase 3).
Until then both features stay `policy-landed`, and `docs/ACCEPTANCE.md` keeps
counting them as such.
