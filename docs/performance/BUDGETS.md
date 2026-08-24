# Performance budgets

**Roadmap item 46.** Privacy work is allowed to cost something; it is not allowed to cost an
unmeasured amount. Every number below is a budget with a measurement method. The phrase
"performance is good" does not appear in this repository, and `scripts/check_perf_claims.py`
fails the build if it or anything like it shows up.

The budget table lives in code (`src_overrides/bedrock/perf/perf_budgets.{h,cc}`) so the test can
enforce it. This document explains it.

## Measured on every commit

`perf_budgets_test.cc` measures these on the CI machine (1 core, no tuning) and fails when a
number exceeds its budget. Typical values from a recent run are in brackets.

| Metric | Budget | Recent | How |
| --- | --- | --- | --- |
| Filter match per request | 20 µs | 0.22 µs | 50,000 rules, 10,000 URLs, mean (`filter_engine_test`) |
| Filter list parse | 2 s / 100k rules | 0.18 s | 100,000 generated rules through `AddList` |
| Full pipeline decision | 30 µs | 0.54 µs | `BlockingPipeline::Evaluate`, 20,000 rules, 10,000 requests |
| CNAME alias lookup on the decision path | 5 µs | 0.18 µs | 4,000 cached aliases, 10,000 eligible requests, mean |
| Tab model operation (200 tabs) | 200 µs | 4.8 µs | open / pin / activate / sleep / duplicate-scan cycle |
| Theme property change | 50 µs | 0.30 µs | set + validate, 10,000 iterations |
| History search over 20,000 visits | 30 ms | 4.5 ms | substring query, mean of 20 |

### What this table already caught

Writing it found a real defect. The filter index picked each rule's **longest** token, which looks
sensible and fails badly: thousands of rules ending in the same popular word all landed in one
bucket, and any request carrying that word walked the whole bucket. A matching request cost
**70 µs** against a 20 µs budget, while a non-matching one cost 0.35 µs — the signature of one
oversized bucket. Indexing by the **rarest** token instead (frequency estimated across the loaded
lists, ties to the longer token) brought it to **0.21 µs**, about 330× faster, with no change to
what matches. See `FilterEngine::Reindex()`.

That is the argument for budgets in general: nobody would have noticed by browsing.

## Pending a real build

These need a browser binary on reference hardware. They are targets, and they are listed as
**pending** — not reported as met — until someone measures them. The list is deliberately visible
rather than dropped.

| Metric | Target | Method |
| --- | --- | --- |
| Cold start to first paint | 1200 ms | cold page cache, empty profile, 10 runs, median |
| Warm start to first paint | 400 ms | warm cache, existing profile, 20 tabs restored lazily |
| New tab to interactive | 150 ms | about:blank, 20 runs, median |
| Idle memory, 1 blank tab | 350 MB | PSS 60 s after startup, no extensions |
| Additional memory per idle tab | 45 MB | 10 identical pages, PSS delta, mean |
| Idle CPU, 10 tabs | 0.5 % of one core | 5 minutes idle |
| Added latency per request | 1 ms | loopback server, 1,000 requests, versus the same build with the pipeline off |
| JS benchmark loss vs stock Chromium | 3 % | Speedometer-class, 5 runs, median, default fingerprint level |

## Design commitments behind the numbers

- **Efficient filter matching** — token index, rarest-token selection, no per-request allocation
  beyond the token vector, one decision point (`BlockingPipeline`) rather than four blockers each
  re-examining the request.
- **Aggressive but safe tab sleeping** — background tabs discarded after an idle threshold, never
  the active, pinned or audible ones (the three exceptions that make the feature tolerable).
- **Low idle CPU** — no polling loops in Bedrock code; the behavioural heuristic deletes a site's
  list once its threshold is reached instead of growing forever.
- **UI rendering** — animations capped at 200 ms and `prefers-reduced-motion` honoured
  (`docs/design/027`); a theme change is a repaint or a relayout, never a restart.
- **Network pipeline** — the privacy stack adds one decision per request, not one per subsystem.

## Adding a metric

Add it to `PerfBudgets::All()` with a limit, a unit, a method and a `how` string precise enough to
repeat. If it can be measured on the host, measure it in `perf_budgets_test.cc`; if it needs a
build, mark it `kRequiresBuild` and it will be listed as pending. A budget without a `how` fails
the test.
