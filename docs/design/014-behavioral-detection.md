# 014 — Behavioral tracker detection

**Roadmap item 14.** Status: landed and host-tested
(`src_overrides/bedrock/privacy/tracker_blocker/tracker_heuristic.{h,cc}`).

## The idea we studied

Privacy Badger's premise, from EFF's public description: a domain is a tracker not because a
list says so, but because it is **observed** storing identifying state across several unrelated
first-party sites. Badger also sends Global Privacy Control and Do Not Track, and strips
tracking parameters from links.

Privacy Badger is GPL-3.0 (`THIRD_PARTY_NOTICES/privacy-badger.txt`): no code, no lists, no
yellow list. The heuristic here is written from the published description.

## How it learns

```
third party stores state on site A   → count 1
                          on site B  → count 2
                          on site C  → count 3 → tracker
```

Design points, each with a test:

- **Repeats do not count.** Distinct first parties only; a site the user reads daily stays one
  observation, so a popular site cannot train the browser against its own widgets.
- **First-party state is never recorded.** `Observe()` drops same-party observations, and the
  pipeline never forwards them.
- **The table is not a history.** Once the threshold is reached the set of first-party sites is
  *deleted*; what remains is the domain and a count. A privacy feature that quietly accumulates
  "you saw tracker.test on clinic.test, bank.test, dating.test" would be worse than the tracker.
- **Nothing leaves the device.** There is no learning server, no shared list, no telemetry —
  Bedrock has no backend at all (roadmap item 4). Export/import exists so the *user* can move or
  inspect the table.
- **Breakage escape hatch.** Domains marked partition-only (login, payment, auth CDNs) are
  partitioned rather than blocked. The user manages that list locally; there is no remote list
  to fetch.
- **Honouring the signal earns trust.** A domain flagged as honouring GPC/DNT is allowed and its
  counter reset — EFF's rule: a tracker that promises to stop is given the chance to keep the
  promise.
- **The user always wins.** An explicit allow/block for a domain beats anything learned, in both
  directions.

## Classification

| Verdict | Meaning |
|---|---|
| `kUnknown` | not enough evidence — the request still faces the cookie policy (usually partitioned) |
| `kAllow` | user rule, or honours privacy signals |
| `kPartition` | loads, no cross-site state |
| `kBlock` | request is not made |

The verdict is consumed only by the [blocking pipeline](013-blocking-pipeline.md) — this layer
blocks nothing by itself (item 13).

## GPC / DNT and link cleaning

`Sec-GPC: 1` and `DNT: 1` are sent unless the user turned tracker protection off for the site: a
signal that contradicts the user's own setting is worse than no signal.

`BlockingPipeline::CleanUrl()` strips click-identifier parameters (`fbclid`, `gclid`, `msclkid`,
`utm_*`, …) from navigations, keeps everything else, and preserves the fragment. The list is
short on purpose: removing a parameter a site actually needs breaks the click, and a broken
click sends the user to another browser — a worse privacy outcome than one surviving `utm_term`.
